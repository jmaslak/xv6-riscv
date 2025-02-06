// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

volatile unsigned long phystop = (KERNBASE + 0x8000000000ul);
volatile int eom_marker = 0;

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

    // We can't trust any local variable, because the registers might
    // have gotten clobbered.
    //
    // So we instead check for the marker. If the marker is NOT set,
    // we are fine to trust our locals again.
    //
    // This isn't a problem because if the locals get clobbered,
    // the marker is set, and we return and who cares about the local
    // then?

    if (eom_marker) { return; }
    if (*p != '0') { return; }  // On the off chance a write doesn't
                                // generate a trap.

    phystop = (unsigned long) p + PGSIZE;
  }
}

// We use this as a trap handler. Now if any traps OTHER than an invalid
// memory access occur, we could be in trouble and undercount our RAM.
//
// We're going to pretend that can't happen.  In real life, we'd
// probably want to check for proper values.
//
// I probably should just have written this in ASM...
void __attribute__((aligned(4), noreturn, naked))
found_last_memory()
{
    eom_marker = 1;
    asm volatile("addi sp,sp,-16");
    asm volatile("sd ra,0(sp)");
    asm volatile("sd x5,8(sp)");
    register uint64 mepc asm ("x5") = r_mepc() + 4;
    w_mepc(mepc);  // Advance to next instruction
    asm volatile("ld x5,8(sp)");
    asm volatile("ld ra,0(sp)");
    asm volatile("addi sp,sp,16");
    asm volatile("mret");
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
