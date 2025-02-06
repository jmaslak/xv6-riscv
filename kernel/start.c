#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();
void calculate_ram();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

extern char end[];

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // Calculate RAM
  calculate_ram();

  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// ask each hart to generate timer interrupts.
void
timerinit()
{
  // enable supervisor-mode timer interrupts.
  w_mie(r_mie() | MIE_STIE);
  
  // enable the sstc extension (i.e. stimecmp).
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // allow supervisor to use stimecmp and time.
  w_mcounteren(r_mcounteren() | 2);
  
  // ask for the very first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}

// Calculate the amount of RAM
// Must be run when we're in machine mode and before we set mret
// privilege to supervisor mode.
void
calculate_ram() {
  volatile static int done = 0;
  int id = r_tp();

  if (!id) {
    // We want to ensure we return from an exception i machine mode.
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_M;
    w_mstatus(x);

    // When we get an exception, we assume it's because we accessed
    // invalid memory. So we're going to set that as our trap
    // handler.
    w_mtvec((uint64)found_last_memory);

    // We run a routine that updates the phystop variable until an
    // found_last_memory runs (during an exception).
    find_last_memory(end);

    // Reset the trap handler.
    w_mtvec(0);
    done = 1;
  } else {
    // We shouldn't need to block the other cores, but lets be safe.
    // We don't bother using a proper lock because done is going to
    // consistenlty read zero just fine until we write to it, and we
    // don't need the write to be atomic (if it takes an extra cycle, so
    // be it).
    while (!done) {}
  }
}
