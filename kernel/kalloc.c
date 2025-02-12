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

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

extern pagetable_t kernel_pagetable;

volatile unsigned long phystop = (KERNBASE + 0x8000000000ul);
volatile unsigned long eom_marker = 0;
void * kheap_start = 0;
void * kheap_next = 0;
struct spinlock kheap_lock;

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

// Finds memory end point
void
find_last_memory(void *pa_start)
{
  volatile char *p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*) 0x10000000000ull; p += PGSIZE) {
    *p = '0';                   // We expect this to generate a trap
                                // when accessing invalid RAM.

    if (eom_marker) { return; } // Did we get the trap?
    phystop = (unsigned long) p + PGSIZE;
  }
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
    kheap_start = (void *) phystop;
    kheap_next = (void *) phystop;
    kheap_grow();
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
