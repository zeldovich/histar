#ifndef JOS_KERN_PAGEINFO_H
#define JOS_KERN_PAGEINFO_H

#include <machine/types.h>
#include <kern/arch.h>
#include <kern/pagetree.h>

struct page_info {
    // references to this page from pagetree's
    uint32_t pi_ref;

    // write-shared references, i.e. pagetree_copy(share_pinned=1)
    uint32_t pi_write_shared_ref;

    // writable hardware refs (DMA, PTE) to this page (if pi_indir == 0)
    // or to child pages of this indir page (if pi_indir == 1)
    uint32_t pi_hw_write_pin;

    // readable hardware refs (DMA, PTE) to this page, only for pi_indir == 0
    // (does not mean anything for indirect pages, when pi_indir == 1)
    uint32_t pi_hw_read_pin;

    uint32_t pi_indir : 1;	// data page or indirect page
    uint32_t pi_dirty : 1;	// eventually reflects PTE dirty bit
    uint32_t pi_freepage : 1;	// page on the free list

    // Indirect parent pagetree page, if any (only when pi_ref == 1).
    struct pagetree_indirect_page *pi_parent;

    // list of pagetree_entry slots referencing this page
    struct pagetree_entry_list pi_plist;

    // Segment ID and offset, the last time this page was mapped.
    uint64_t pi_seg;
    uint64_t pi_segpg;
};

extern struct page_info *page_infos;

static __inline struct page_info *
page_to_pageinfo(void *p)
{
    // No page infos if phys page is higher than global_npages
    if ((kva2pa(p) >> PGSHIFT) > global_npages)
        return NULL;
    ppn_t pn = pa2ppn(kva2pa(p));
    return &page_infos[pn];
}

#endif
