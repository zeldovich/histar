#ifndef JOS_KERN_PAGETREE_H
#define JOS_KERN_PAGETREE_H

#include <machine/types.h>
#include <machine/memlayout.h>

typedef union {
    void *page;
    uint64_t flags;
} pagetree_entry;

#define PAGETREE_FLAG_MASK	0xfff
#define PAGETREE_COW		0x01
#define PAGETREE_RO		0x02

static __inline __attribute__((always_inline)) void *
pagetree_entry_page(pagetree_entry e)
{
    return (void*) (e.flags & ~PAGETREE_FLAG_MASK);
}

#define PAGETREE_DIRECT_PAGES		28
#define PAGETREE_INDIRECTS		4
#define PAGETREE_ENTRIES_PER_PAGE	(PGSIZE / sizeof(pagetree_entry))

/*
 * The page tree can store slightly over 512^PAGETREE_INDIRECTS pages.
 * Currently it supports a single-level COW snapshot.
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

// Create a read-only clone of src into dst
void pagetree_clone(struct pagetree *src, struct pagetree *dst);

// Free a clone, including any pages that have been COW'ed in the base
void pagetree_clone_free(struct pagetree *clone, struct pagetree *base);

// Free the pagetree, including all of the pages
void pagetree_free(struct pagetree *pt);

// Get a page currently stored in the page tree
int  pagetree_get_page(struct pagetree *pt, uint64_t npage, void **pagep,
		       page_rw_mode rw);

// Put a page into the page tree
int  pagetree_put_page(struct pagetree *pt, uint64_t npage, void *page);

// Max number of pages in a pagetree
uint64_t pagetree_maxpages();

#endif
