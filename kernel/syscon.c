#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "defs.h"

uint16   syscon_shutdown = 0;
uint16   syscon_reboot   = 0;
uint16 * syscon          = 0;

extern pagetable_t kernel_pagetable;

void syscon_init() {
    printf("syscon: ");
    if (syscon) {
        print_hex64("", (uint64) syscon, 0);
        kvmmap(kernel_pagetable, (uint64) syscon, (uint64) syscon, PGSIZE, PTE_R | PTE_W);
    }

    if (syscon && (syscon_shutdown || syscon_reboot)) {
        printf(" supports");
        if (syscon_shutdown)
            printf(" [shutdown]");
        if (syscon_reboot)
            printf(" [reboot]");
        printf("\n");
    } else {
        printf("not usable\n");
    }
}

int syscon_shutdown_now() {
    if (syscon && syscon_shutdown) {
        *syscon = syscon_shutdown;
        return 0;
    }
    return 1;
}

int syscon_reboot_now() {
    if (syscon && syscon_reboot) {
        *syscon = syscon_reboot;
        return 0;
    }
    return -1;
}

uint64
sys_shutdown(void)
{
    int type;
    argint(0, &type);

    switch (type) {
        case 0:
            return syscon_shutdown_now();
        case 1:
            return syscon_reboot_now();
    }
    return -1;
}

