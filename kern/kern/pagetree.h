#ifndef JOS_KERN_PAGETREE_H
#define JOS_KERN_PAGETREE_H

#include <machine/types.h>
#include <machine/memlayout.h>

struct pagetree_page {
    uint32_t pg_ref;	// references to this page from pagetree's
    uint32_t pg_pin;	// hardware refs (DMA, PTE) -- for later use
    uint32_t pg_indir;	// data page or indirect page
};

extern struct pagetree_page *pt_pages;

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

typedef enum {
    page_ro,
    page_rw
} page_rw_mode;

// Fancy API for zeroing out
void pagetree_init(struct pagetree *pt);

// Copy (with COW) src into dst
void pagetree_copy(struct pagetree *src, struct pagetree *dst);

// Free the pagetree, including all of the pages (that aren't shared)
void pagetree_free(struct pagetree *pt);

// Get a page currently stored in the page tree
int  pagetree_get_page(struct pagetree *pt, uint64_t npage, void **pagep,
		       page_rw_mode rw);

// Put a page into the page tree
int  pagetree_put_page(struct pagetree *pt, uint64_t npage, void *page);

// Max number of pages in a pagetree
uint64_t pagetree_maxpages(void);

#endif
