#ifndef JOS_KERN_ARCH_H
#define JOS_KERN_ARCH_H

#include <machine/types.h>
#include <machine/memlayout.h>

/*
 * Page allocation
 */
extern size_t global_npages;

extern struct page_stats {
    uint64_t pages_used;
    uint64_t pages_avail;
    uint64_t allocations;
    uint64_t failures;
} page_stats;

int  page_alloc(void **p)
    __attribute__ ((warn_unused_result));
void page_free (void *p);

/*
 * Miscellaneous
 */
void machine_reboot(void);

#endif
