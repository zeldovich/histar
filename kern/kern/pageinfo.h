#ifndef JOS_KERN_PAGEINFO_H
#define JOS_KERN_PAGEINFO_H

#include <machine/types.h>

struct page_info {
    uint32_t pi_ref;		// references to this page from pagetree's
    uint32_t pi_pin;		// hardware refs (DMA, PTE) -- for later use
    uint32_t pi_indir : 1;	// data page or indirect page
    uint32_t pi_dirty : 1;	// eventually reflects PTE dirty bit
};

extern struct page_info *page_infos;

static __inline struct page_info *
page_to_pageinfo(void *p)
{
    ppn_t pn = pa2ppn(kva2pa(p));
    return &page_infos[pn];
}

#endif
