// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "kalloc.h"

#define KM_MAGIC ((struct malloc_struct *) 0x6677aadeul)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

extern pagetable_t kernel_pagetable;

struct malloc_struct {
  unsigned long size;
  struct malloc_struct * next;
  void * data;
};

// 1 MB is the max allocation supported
#define MALLOC_MAX 1048576

// We "bucket" malloc into various sizes, based on the requested space.
#define malloc_size(x) x>524288 ? 1048576 : \
                       x>262144 ?  524288 : \
                       x>131072 ?  262144 : \
                       x> 65536 ?  131072 : \
                       x> 32768 ?   65536 : \
                       x> 16384 ?   32786 : \
                       x>  8192 ?   16384 : \
                       x>  4096 ?    8192 : \
                       x>  2048 ?    4096 : \
                       x>  1024 ?    2048 : \
                       x>   512 ?    1024 : \
                       x>   256 ?     512 : \
                       x>   128 ?     256 : \
                       x>    64 ?     128 : \
                       x>    32 ?      64 : \
                       x>    16 ?      32 : \
                                       16

#define malloc_index(x) x>524288 ? 16 : \
                        x>262144 ? 15 : \
                        x>131072 ? 14 : \
                        x> 65536 ? 13 : \
                        x> 32768 ? 12 : \
                        x> 16384 ? 11 : \
                        x>  8192 ? 10 : \
                        x>  4096 ?  9 : \
                        x>  2048 ?  8 : \
                        x>  1024 ?  7 : \
                        x>   512 ?  6 : \
                        x>   256 ?  5 : \
                        x>   128 ?  4 : \
                        x>    64 ?  3 : \
                        x>    32 ?  2 : \
                        x>    16 ?  1 : \
                                    0

#define MALLOC_MAX_INDEX 16

volatile unsigned long phystop = (KERNBASE + 0x8000000000ul);
volatile unsigned long eom_marker = 0;
void * kheap_start = 0;
void * kheap_next = 0;
void * kheap_last_used = 0;
struct malloc_struct * kmalloc_next[MALLOC_MAX_INDEX+1];  // Next free space which can be allocated
struct spinlock kheap_lock;
struct spinlock kmalloc_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)phystop);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= phystop)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
meminfo(struct mem_info * mem_info) {
    uint64 pages = 0;
    struct run * current = kmem.freelist;

    acquire(&kmem.lock);
    while (current != 0) {
        pages++;
        current = current->next;
    }
    release(&kmem.lock);

    mem_info->total_mem = phystop-KERNBASE;
    mem_info->avail_mem = pages * PGSIZE;
    mem_info->kheap_mem = kheap_next - kheap_start;
}

uint64
sys_meminfo() {
    uint64 user_addr;
    struct proc *p = myproc();
    struct mem_info mem_info;

    argaddr(0, &user_addr);

    meminfo(&mem_info);

    if(copyout(p->pagetable, user_addr, (char *) &mem_info, sizeof(mem_info)) < 0)
        return -1;

    return 0;
}

void kheap_init() {
    initlock(&kheap_lock, "kheap");
    initlock(&kmalloc_lock, "kmalloc");
    kheap_start = (void *) phystop;
    kheap_next = (void *) phystop;
    kheap_last_used = kheap_start - 1;
    for (int i=0; i<MALLOC_MAX_INDEX+1; i++) kmalloc_next[i] = 0ul;
    printf("kheap initialized\n");
}

// Grow the heap by one page (4096 bytes)
void kheap_grow() {
    void * next_phy;

    acquire(&kheap_lock);

    if ((next_phy = (void *) kalloc()) == 0)
        panic("kheap_grow: out of kernel heap space");

    kvmmap(kernel_pagetable, (uint64) kheap_next, (uint64) next_phy, PGSIZE, PTE_R | PTE_W);
    kheap_next += PGSIZE;

    sfence_vma();

    release(&kheap_lock);
}

void * kmalloc(unsigned long size) {
  // Enough for a 8 byte size and an 8 byte pointer, aligned at a 16 byte
  // boundary.
  if (size > MAX_KMALLOC) panic("kmalloc: attempted to allocate too much space");
  if (size == 0) panic("kmalloc: attempted to allocate too little space");

  size = malloc_size(size);
  int index = malloc_index(size);
  unsigned int size_all = size + 16;

  acquire(&kmalloc_lock);
  struct malloc_struct ** current = &kmalloc_next[index];

  while (1) {
    if (!*current) {
      while (kheap_next - kheap_last_used < size_all) {
          kheap_grow();
      }
      struct malloc_struct * newstruct = kheap_last_used + 1;
      kheap_last_used += size_all;
      newstruct->size = size_all;
      newstruct->next = 0;

      *current = newstruct;
    }

    if ((*current)->size == size_all) {
      // Exact size match! (or close enough)
      struct malloc_struct * ptr = *current;
      *current = ptr->next;
      ptr->next = KM_MAGIC;
      release(&kmalloc_lock);
      return &(ptr->data);
    }

    if ((*current)->size > size_all) {
      // Split it!
      struct malloc_struct * ptr = (*current);
      *current = ((void *) ptr) + size_all;
      (*current)->size = ptr->size - size_all;
      (*current)->next = ptr->next;
      ptr->size = size_all;
      ptr->next = KM_MAGIC;
      release(&kmalloc_lock);
      return &(ptr->data);
    }

    current = &((*current)->next);
  }
}

void kmfree(void * ptr) {
  if (((unsigned long) ptr) % 8) panic("kmfree: attempt to free wrongly aligned pointer");
  if (((unsigned long) ptr) <= 16) panic("kmfree: attempt to free wrongly aligned pointer");

  struct malloc_struct * current = ptr - 16;
  if (current->next != KM_MAGIC) panic("kmfree: bad magic");

  int sz = current->size;
  memset((void *)current, 0, malloc_index(sz));
  current->size = sz;

  acquire(&kmalloc_lock);
  int index = malloc_index(sz);

  current->next = kmalloc_next[index];
  kmalloc_next[index] = current;
  release(&kmalloc_lock);
}
