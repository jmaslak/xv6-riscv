#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

#define MB(x) (x/(1024lu * 1024lu))

int
main(int argc, char *argv[])
{
    struct mem_info mem_info;
    meminfo(&mem_info);

    printf("Time: %ld\n", time());
    printf("Total memory: %lu bytes (%lu MB)\n", mem_info.total_mem, MB(mem_info.total_mem));
    printf("Free memory : %lu bytes (%lu MB)\n", mem_info.avail_mem, MB(mem_info.avail_mem));

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
