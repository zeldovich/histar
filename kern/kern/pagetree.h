#ifndef JOS_KERN_PAGETREE_H
#define JOS_KERN_PAGETREE_H

#include <machine/types.h>
#include <machine/memlayout.h>
#include <inc/safetype.h>

typedef struct {
    void *page;
} pagetree_entry;

#define PAGETREE_DIRECT_PAGES		28
#define PAGETREE_INDIRECTS		4
#define PAGETREE_ENTRIES_PER_PAGE	(PGSIZE / sizeof(pagetree_entry))

/*
 * The page tree can store slightly over 512^PAGETREE_INDIRECTS pages.
 */

struct pagetree {
    pagetree_entry pt_direct[PAGETREE_DIRECT_PAGES];
    pagetree_entry pt_indirect[PAGETREE_INDIRECTS];
};

struct pagetree_indirect_page {
    pagetree_entry pt_entry[PAGETREE_ENTRIES_PER_PAGE];
};

typedef SAFE_TYPE(int) page_rw_mode;
#define page_ro	    SAFE_WRAP(page_rw_mode, 1)
#define page_rw	    SAFE_WRAP(page_rw_mode, 2)

// Fancy API for zeroing out
void pagetree_init(struct pagetree *pt);

// Copy (with COW) src into dst
void pagetree_copy(const struct pagetree *src, struct pagetree *dst);

// Free the pagetree, including all of the pages (that aren't shared)
void pagetree_free(struct pagetree *pt);

// Get a page currently stored in the page tree
int  pagetree_get_page(struct pagetree *pt, uint64_t npage, void **pagep,
		       page_rw_mode rw)
    __attribute__ ((warn_unused_result));

// Put a page into the page tree
int  pagetree_put_page(struct pagetree *pt, uint64_t npage, void *page)
    __attribute__ ((warn_unused_result));

// Max number of pages in a pagetree
uint64_t pagetree_maxpages(void);

#endif
