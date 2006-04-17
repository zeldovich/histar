#ifndef JOS_KERN_PAGEINFO_H
#define JOS_KERN_PAGEINFO_H

#include <machine/types.h>
#include <kern/pagetree.h>

struct page_info {
    // references to this page from pagetree's
    uint32_t pi_ref;

    // writable hardware refs (DMA, PTE) to this page (if pi_indir == 0)
    // or to child pages of this indir page (if pi_indir == 1)
    uint32_t pi_pin;

    uint32_t pi_indir : 1;	// data page or indirect page
    uint32_t pi_dirty : 1;	// eventually reflects PTE dirty bit

    // Indirect parent pagetree page, if any (only when pi_ref == 1).
    struct pagetree_indirect_page *pi_parent;
};

extern struct page_info *page_infos;

static __inline struct page_info *
page_to_pageinfo(void *p)
{
    ppn_t pn = pa2ppn(kva2pa(p));
    return &page_infos[pn];
}

#endif
