// Memory allocation routines

#ifndef __KALLOC_H__
#define __KALLOC_H__

#define MAX_KMALLOC (1048576ul)

struct mem_info {
    uint64 total_mem;
    uint64 avail_mem;
    uint64 kheap_mem;
};

#endif
