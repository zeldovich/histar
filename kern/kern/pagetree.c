#include <machine/pmap.h>
#include <kern/pagetree.h>
#include <kern/lib.h>
#include <inc/error.h>

void
pagetree_init(struct pagetree *pt)
{
    memset(pt, 0, sizeof(*pt));
}

void
pagetree_clone(struct pagetree *src, struct pagetree *dst)
{
    memcpy(dst, src, sizeof(*dst));

    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++) {
	dst->pt_direct[i].flags |= PAGETREE_RO;
	src->pt_direct[i].flags |= PAGETREE_COW;
    }

    for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	dst->pt_indirect[i].flags |= PAGETREE_RO;
	src->pt_indirect[i].flags |= PAGETREE_COW;
    }
}

static void
pagetree_free_ent(pagetree_entry *clone_ent, pagetree_entry *base_ent, int level)
{
    void *clone_page = pagetree_entry_page(*clone_ent);
    void *base_page = base_ent ? pagetree_entry_page(*base_ent) : 0;

    if (base_ent)
	assert((clone_ent->flags & PAGETREE_RO));
    else
	assert(!(clone_ent->flags & (PAGETREE_RO | PAGETREE_COW)));

    if (base_ent && (base_ent->flags & PAGETREE_COW)) {
	assert(clone_page == base_page);
	base_ent->flags &= ~PAGETREE_COW;
	clone_ent->page = 0;
	return;
    }

    if (clone_page) {
	if (level > 0) {
	    struct pagetree_indirect_page *cip = clone_page;
	    struct pagetree_indirect_page *bip = base_page;

	    for (int i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++)
		pagetree_free_ent(&cip->pt_entry[i],
				  bip ? &bip->pt_entry[i] : 0,
				  level - 1);
	}

	page_free(clone_page);
	clone_ent->page = 0;
    }
}

void
pagetree_clone_free(struct pagetree *clone, struct pagetree *base)
{
    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	pagetree_free_ent(&clone->pt_direct[i],
			  base ? &base->pt_direct[i] : 0, 0);

    for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	pagetree_free_ent(&clone->pt_indirect[i],
			  base ? &base->pt_indirect[i] : 0, i+1);

    pagetree_init(clone);
}

void
pagetree_free(struct pagetree *pt)
{
    pagetree_clone_free(pt, 0);
}

static int
pagetree_ent_cow(pagetree_entry *ent)
{
    assert(!(ent->flags & PAGETREE_RO));

    if (ent->flags & PAGETREE_COW) {
	void *page = pagetree_entry_page(*ent);
	void *cow = 0;

	if (page) {
	    int r = page_alloc(&cow);
	    if (r < 0)
		return r;
	    memcpy(cow, page, PGSIZE);
	}

	// also clears PAGETREE_COW
	ent->page = cow;
    }

    return 0;
}

static int
pagetree_get_entp_indirect(pagetree_entry *indir, uint64_t npage,
			   pagetree_entry **outp, page_rw_mode rw,
			   int level)
{
    if (rw == page_rw)
	pagetree_ent_cow(indir);

    struct pagetree_indirect_page *pip = pagetree_entry_page(*indir);
    if (!pip) {
	if (rw == page_ro) {
	    *outp = 0;
	    return 0;
	}

	void *page;
	int r = page_alloc(&page);
	if (r < 0)
	    return r;

	memset(page, 0, PGSIZE);
	indir->page = page;
	pip = page;
    }

    if (level == 0) {
	*outp = &pip->pt_entry[npage];
	return 0;
    }

    uint64_t n_pages_per_pip_entry = 1;
    for (int i = 0; i < level; i++)
	n_pages_per_pip_entry *= PAGETREE_ENTRIES_PER_PAGE;

    int next_slot = npage / n_pages_per_pip_entry;
    int next_page = npage % n_pages_per_pip_entry;

    assert(next_slot < PAGETREE_ENTRIES_PER_PAGE);
    return pagetree_get_entp_indirect(&pip->pt_entry[next_slot],
				      next_page, outp, rw, level - 1);
}

static int
pagetree_get_entp(struct pagetree *pt, uint64_t npage,
		  pagetree_entry **entp, page_rw_mode rw)
{
    if (npage < PAGETREE_DIRECT_PAGES) {
	*entp = &pt->pt_direct[npage];
	return 0;
    }
    npage -= PAGETREE_DIRECT_PAGES;

    uint64_t num_indirect_pages = 1;
    for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	num_indirect_pages *= PAGETREE_ENTRIES_PER_PAGE;
	if (npage < num_indirect_pages)
	    return pagetree_get_entp_indirect(&pt->pt_indirect[i],
					      npage, entp, rw, i);
	npage -= num_indirect_pages;
    }

    cprintf("pagetree_get_entp: %ld leftover!\n", npage);
    return -E_NO_SPACE;
}

int
pagetree_get_page(struct pagetree *pt, uint64_t npage,
		  void **pagep, page_rw_mode rw)
{
    pagetree_entry *ent;
    int r = pagetree_get_entp(pt, npage, &ent, rw);
    if (r < 0)
	return r;

    void *page = ent ? pagetree_entry_page(*ent) : 0;
    if (rw == page_ro || page == 0) {
	*pagep = page;
	return 0;
    }

    if (ent->flags & PAGETREE_RO)
	panic("writing to a pagetree clone: pt %p page %ld",
	      pt, npage);

    pagetree_ent_cow(ent);
    *pagep = pagetree_entry_page(*ent);
    return 0;
}

int
pagetree_put_page(struct pagetree *pt, uint64_t npage, void *page)
{
    pagetree_entry *ent;
    int r = pagetree_get_entp(pt, npage, &ent, page_rw);
    if (r < 0)
	return r;

    assert(ent != 0);
    void *cur = pagetree_entry_page(*ent);
    if (cur && !(ent->flags & PAGETREE_COW))
	page_free(cur);

    ent->page = page;
    return 0;
}

uint64_t
pagetree_maxpages()
{
    uint64_t npages = PAGETREE_DIRECT_PAGES;

    uint64_t indirs = 1;
    for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	indirs *= PAGETREE_ENTRIES_PER_PAGE;
	npages += indirs;
    }

    return npages;
}
