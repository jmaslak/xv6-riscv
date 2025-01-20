#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("Time: %ld\n", time());
    printf("sizeof(int) : %lu bytes\n", sizeof(int));
    printf("sizeof(long): %lu bytes\n", sizeof(long));
    exit(0);
}
