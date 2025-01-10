#include "kernel/types.h"
// #include "kernel/fcntl.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("Hello, World. %20s foo\n", "a");
    printf("Hello, World. %-20s foo\n", "a");
    exit(0);
}
