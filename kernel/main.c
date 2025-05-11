    #include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "byteswap.h"
#include "spinlock.h"
#include "proc.h"

volatile static int started = 0;
extern struct cpu cpus[NCPU];

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    walk_dtb();
    printf("memory: %lu MB\n", (phystop-KERNBASE) / (1024*1024));
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    kheap_init();    // initialize heap
    rtcinit();       // initialize RTC
    syscon_init();   // initialize qemu-virt syscon driver
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    __sync_synchronize();
    printf("%u hart%s detected\n", cpu_count, cpu_count == 1 ? "" : "s");
    if (cpu_count > 1) printf("starting other hart%s\n", cpu_count > 2 ? "s" : "");
    started = 1;
    cpus[0].started = 1;

    int running = 1;
    while (running < cpu_count) {
      running = 1;
      for (int i=1; i<cpu_count; i++) {
        if (cpus[i].started) running++;
      }
    }

    printf("all harts started\n");
    userinit();      // first user process
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
    cpus[cpuid()].started = 1;
  }

  scheduler();        
}
