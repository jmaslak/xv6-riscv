#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

struct {
    char help;
    char reboot;
} opts;

int parse_args(int, char **);
void help(char *);

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv)) {
        help(argv[0]);
        exit(1);
    }
    if (opts.help) {
        help(argv[0]);
        exit(0);
    }

    if (opts.reboot) {
        printf("rebooting now...\n");
        if (shutdown(1)) {
            printf("Reboot failed!\n");
            exit(1);
        } else {
            printf("We already should be rebooted...");
            exit(1);
        }
    } else {
        printf("shutdown now...\n");
        if (shutdown(0)) {
            printf("Shutdown failed!\n");
            exit(1);
        } else {
            printf("We already should be shut down...");
            exit(1);
        }
    }
}

int parse_args(int argc, char *argv[]) {
    int current = 1;

    opts.help   = 0;
    opts.reboot = 0;

    while (current < argc) {
        if (!strcmp(argv[current], "-h"))
            opts.help = 1;
        else if (!strcmp(argv[current], "-r"))
            opts.reboot = 1;
        else
            return -1;

        current++;
    }

    return 0;
}

void help(char * prog) {
    printf("%s [-h] [-r]:\n", prog);
    printf("  -h  display help\n");
    printf("  -r  reboot instead of shutdown\n");
    printf("\n");
}
