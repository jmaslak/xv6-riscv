//
// Time system calls
//

#include "types.h"
#include "riscv.h"
#include "defs.h"

uint64
sys_nanotime(void)
{
  return get_rtc_nanotime();
}
