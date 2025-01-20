#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("Time: %ld\n", time());
    printf("Hello, World. %20s foo\n", "a");
    printf("Hello, World. %-20s foo\n", "a");

    printf("%lu %lu\n", sizeof(long), sizeof(int));
    exit(0);
}
