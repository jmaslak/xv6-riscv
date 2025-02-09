#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    printf("Time: %ld\n", time());
    printf("sizeof(int) : %lu bytes\n", sizeof(int));
    printf("sizeof(long): %lu bytes\n", sizeof(long));

    char * o = malloc(sizeof(char) * 6);
    int i = optimist(o);
    if (!i) {
        printf("Optimist: %s\n", o);
    } else {
        fprintf(stderr, "Oh no, we have a failure! Return value of optimist(): %d\n", i);
        exit(1);
    }

    for (int i=0; i<argc; i++) {
        printf("Arg %d: %s\n", i, argv[i]);
    }

    exit(0);
}
