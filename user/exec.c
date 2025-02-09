#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Provide application name and argv[0] as a minimum\n");
        exit(1);
    }

    exec(argv[0], argv+1);
    fprintf(stderr, "Exec failed.");
}
