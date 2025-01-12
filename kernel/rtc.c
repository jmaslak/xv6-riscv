#include "types.h"
#include "riscv.h"
#include "defs.h"

// This code was inspired by:
// https://github.com/amirR01/xv6-improvments/commit/b9d9c5a430cff4cc0626b7240b35abc2c6e31b88

void rtcinit() {
    printf("rtc: %ld seconds\n", get_rtc_nanotime() / 1000000000);
}

uint64 GoldFish__rtc__get_nanotime(void) {
    uint64* epoch_pnt = (uint64*) 0x101000;
    return *epoch_pnt;
}

uint64 get_rtc_nanotime(void) {
    return GoldFish__rtc__get_nanotime();
}
