// DTB parsing stuff
//
// This must be used BEFORE kinit() is called!

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "byteswap.h"

struct dtb_header {
    uint32 magic;
    uint32 totalsize;
    uint32 off_dt_struct;
    uint32 off_dt_strings;
    uint32 off_mem_rsvmap;
    uint32 version;
    uint32 last_comp_version;
    uint32 boot_cpuid_phys;
    uint32 size_dt_strings;
    uint32 size_dt_struct;
};

void gender_dtb_header(struct dtb_header *);

inline uint32 * advance_past_value(uint32 * tag, uint32 len) {
    return (uint32*) ALIGN4(((uint64) tag) + len);
}

inline char * get_string(struct dtb_header * header, uint32 offset) {
    return ((char *) header) + header->off_dt_strings + offset;
}

void walk_dtb() {
    struct dtb_header * header;
    header = *(struct dtb_header **) DTB_PTR;
    gender_dtb_header(header);

    print_hex64("fdt @ ", (uint64) header, 0);
    print_hex32(", magic: ", header->magic, 0);
    printf("\n");
    if (header->magic != 0xd00dfeed)
        panic("walk_dtb: bad magic");

    print_hex32("  total size       : ", header->totalsize, 1);
    print_hex32("  offset dt_struct : ", header->off_dt_struct, 1);
    print_hex32("  offset dt_strings: ", header->off_dt_strings, 1);
    print_hex32("  off_mem_rsvmap   : ", header->off_mem_rsvmap, 1);
    print_hex32("  version          : ", header->version, 1);
    print_hex32("  compat version   : ", header->last_comp_version, 1);
    print_hex32("  boot cpuid       : ", header->boot_cpuid_phys, 1);
    print_hex32("  size dt_strings  : ", header->size_dt_strings, 1);
    print_hex32("  size dt_struct   : ", header->size_dt_struct, 1);

    if (header->off_dt_strings + header->size_dt_strings > header->totalsize)
        panic("walk_dtb: dt_strings outside devicetree");
    if (header->off_dt_struct + header->size_dt_struct > header->totalsize)
        panic("walk_dtb: dt_struct outside devicetree");
    if (header->version < 17)
        panic("walk_dtb: version less than 17");
    if (header->last_comp_version > 17)
        panic("walk_dtb: compat version greater than 17");

    char * min_string = header->off_dt_strings + (char *)header;
    char * oob_string = header->off_dt_strings + (char *)header + header->size_dt_strings;

    uint32 * tag     = (uint32*) ((uint64) header + header->off_dt_struct);
    // uint32 * oob_tag = (uint32*) ((uint64) header + header->off_dt_struct + header->size_dt_struct);

    int done = 0;
    int depth = 0;
    uint32 t = swap_u32(*tag);
    char * s = "";
    while (!done) {
        if (t != 2) for (int i=0; i<depth; i++) printf(" ");
        tag++;

        switch (t) {
            case 1: // FDT_BEGIN_NODE
                depth++;
                s = (char *) tag;
                if (s[0] == '\0')
                    printf("    node <root node>:\n");
                else
                    printf("    node %s:\n", s);
                tag = (uint32*) ALIGN4(((uint64) tag) + 1 + strlen(s));
                break;
            case 2: // FDT_END_NODE
                if (--depth < 0)
                    panic("node end!");
                break;
            case 3: // FDT_PROP
                uint32 len  = swap_u32(*tag); tag++;
                uint32 nm   = swap_u32(*tag); tag++;
                if (min_string + nm + 1 >= oob_string)
                    panic("dtb_walk: string out of bounds");

                printf("    prop %s (len: %u)", min_string + nm, len);
                if (len == 0) {
                    // Do nothing
                } else if (strncmp(min_string + nm, "compatible", len) == 0) {
                    printf(" =");
                    for (int j=0; j<len;) {
                        if (*(j+(char*) tag) != '\0') {
                            printf(" %s", j+(char*) tag);
                            j += strlen(j+(char*) tag);
                        }
                        j++;
                    }
                } else if (strncmp(min_string + nm, "phandle", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "value", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "offset", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "regmap", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "interrupts", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "cpu", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "interrupt_parent", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "#address-cells", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "#size-cells", len) == 0) {
                    print_hex32(" = ", swap_u32(*tag), 0);
                } else if (strncmp(min_string + nm, "model", len) == 0) {
                    printf(" = %s", (char *) tag);
                } else if (strncmp(min_string + nm, "reg", len) == 0) {
                    if (len == 16) {
                        uint64 start  = swap_u64(*(uint64*) tag);
                        uint64 offset = swap_u64(*(1+(uint64*) tag));
                        print_hex64(" = ", start, 0);
                        print_hex64(" ", offset, 0);

                        if ((strlen(s) > 7) && (!strncmp("memory@", s, 7))) {
                            if (start == KERNBASE) {
                                phystop = KERNBASE + offset;
                            }
                        }
                    }
                }

                printf("\n");
                tag = advance_past_value(tag, len);
                break;
            case 4: // FDT_NOP
                printf("    noop\n");
                break;
            case 9: // FDT_END
                done = 1;
                printf("  <fdt end>\n");
                break;
            default:
                printf("tag: %u\n", t);
                panic("dtb_walk: unknown tag");
        }

        t = swap_u32(*tag);
    }
}

void gender_dtb_header(struct dtb_header * header) {
    header->magic = swap_u32(header->magic);
    header->totalsize = swap_u32(header->totalsize);
    header->off_dt_struct = swap_u32(header->off_dt_struct);
    header->off_dt_strings = swap_u32(header->off_dt_strings);
    header->off_mem_rsvmap = swap_u32(header->off_mem_rsvmap);
    header->version = swap_u32(header->version);
    header->last_comp_version = swap_u32(header->last_comp_version);
    header->boot_cpuid_phys = swap_u32(header->boot_cpuid_phys);
    header->size_dt_strings = swap_u32(header->size_dt_strings);
    header->size_dt_struct = swap_u32(header->size_dt_struct);
}
