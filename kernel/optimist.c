#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"

// 1 in N chance:
#define OPTIMIST_CHANCE 8

// A modifie XTEA algorithm.
// Taken from public domain code at https://en.wikipedia.org/wiki/XTEA
// retrieved Jan 19, 2025 (by David Wheeler & Roger Needham).
uint64 encipher(unsigned int num_rounds, uint64 input, uint const key[4]) {
    uint i;
    uint v0 = input && 0xffffffff;
    uint v1 = input >> 32;
    uint sum=0, delta=0x9E3779B9;
    for (i=0; i < num_rounds; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
    }
    return (((long)v0) << 32) + ((long)v1);
}

// Returns "yes" to the pointer address provided by the user most of the
// time, but returns "maybe" occasionally.
uint64
sys_optimist(void)
{
  uint64 p;
  argaddr(0, &p);

  // Random number generation:
  //   We use the XTEA algorithm with a key that partially consists of
  //   the process PID and an input of the RTC nanosecond timer.
  uint key[] = {512, 111, 93, myproc()->pid};
  uint64 entropy = encipher(64, get_rtc_nanotime(), key);

  if (entropy % OPTIMIST_CHANCE) {
    return copyout(myproc()->pagetable, p, "yes", 4);
  } else {
    return copyout(myproc()->pagetable, p, "maybe", 5);
  }
}
