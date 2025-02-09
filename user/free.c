#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

#define MB(x) (x/(1024lu * 1024lu))
#define KB(x) (x/1024lu)

struct {
    char help;
    char mb;
} opts;

int parse_args(int, char **);
void help(char *);
void print_uint64(uint64, uint);

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv)) {
        help(argv[0]);
        exit(1);
    }
    if (opts.help) {
        help(argv[0]);
        exit(0);
    }

    struct mem_info mem_info;
    meminfo(&mem_info);

    uint64 total = mem_info.total_mem;
    uint64 free  = mem_info.avail_mem;
    uint64 used  = total - free;

    if (opts.mb) {
        total = MB(total);
        free  = MB(free);
        used  = MB(used);
    } else {
        total = KB(total);
        free  = KB(free);
        used  = KB(used);
    }

    printf("               total        used        free\n");

    printf("        ");
    print_uint64(total, 12);
    print_uint64(used, 12);
    print_uint64(free, 12);
    printf("\n");

    exit(0);
}

int parse_args(int argc, char *argv[]) {
    int current = 1;

    opts.help = 0;
    opts.mb = 0;

    while (current < argc) {
        if (!strcmp(argv[current], "-h"))
            opts.help = 1;
        else if (!strcmp(argv[current], "-m"))
            opts.mb = 1;
        else
            return -1;

        current++;
    }

    return 0;
}

void print_uint64(uint64 num, uint places) {
    uint digits = (num == 0) ? 1 : 0;

    for (int limit=1; limit <= num; limit *=10, digits++);
    while (digits++ < places)
        write(1, " ", 1);

    printf("%lu", num);
}

void help(char * prog) {
    printf("%s [-h] [-m]:\n", prog);
    printf("  -h  display help\n");
    printf("  -m  display in megabytes (rather than bytes)\n");
    printf("\n");
}
