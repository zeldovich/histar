#include <machine/pmap.h>
#include <kern/pagetree.h>
#include <kern/pageinfo.h>
#include <kern/lib.h>
#include <inc/error.h>

static void pagetree_decref(void *p);

static void
pagetree_free_page(void *p)
{
    struct page_info *ptp = page_to_pageinfo(p);
    assert(ptp->pi_ref == 0);
    assert(ptp->pi_pin == 0);

    if (ptp->pi_indir) {
	struct pagetree_indirect_page *pip = p;
	for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++) {
	    if (pip->pt_entry[i].page) {
		pagetree_decref(pip->pt_entry[i].page);
		pip->pt_entry[i].page = 0;
	    }
	}
    }

    memset(ptp, 0, sizeof(*ptp));
    page_free(p);
}

static void
pagetree_decref(void *p)
{
    struct page_info *ptp = page_to_pageinfo(p);
    if (--ptp->pi_ref == 0)
	pagetree_free_page(p);
}

static void
pagetree_incref(void *p)
{
    struct page_info *ptp = page_to_pageinfo(p);
    ptp->pi_ref++;
}

static int pagetree_cow(pagetree_entry *ent);

static int __attribute__ ((warn_unused_result))
pagetree_indir_copy(void *src, void *dst)
{
    struct pagetree_indirect_page *pdst = dst;

    for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++)
	if (pdst->pt_entry[i].page)
	    page_to_pageinfo(pdst->pt_entry[i].page)->pi_ref++;

    for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++) {
	if (pdst->pt_entry[i].page &&
	    page_to_pageinfo(pdst->pt_entry[i].page)->pi_pin) {
	    int r = pagetree_cow(&pdst->pt_entry[i]);
	    if (r < 0)
		return r;
	}
    }

    assert(page_to_pageinfo(src)->pi_indir);
    page_to_pageinfo(dst)->pi_indir = 1;
}

static int __attribute__ ((warn_unused_result))
pagetree_cow(pagetree_entry *ent)
{
    if (!ent->page)
	return 0;

    struct page_info *ptp = page_to_pageinfo(ent->page);
    assert(ptp->pi_ref > 0);

    if (ptp->pi_ref > 1) {
	void *copy;

	int r = page_alloc(&copy);
	if (r < 0)
	    return r;

	assert(page_to_pageinfo(copy)->pi_ref == 0);
	assert(page_to_pageinfo(copy)->pi_pin == 0);
	memcpy(copy, ent->page, PGSIZE);
	pagetree_incref(copy);

	if (ptp->pi_indir) {
	    r = pagetree_indir_copy(ent->page, copy);
	    if (r < 0) {
		pagetree_decref(copy);
		return r;
	    }
	}

	page_to_pageinfo(copy)->pi_dirty = ptp->pi_dirty;
	pagetree_decref(ent->page);
	ent->page = copy;
    }

    return 0;
}

void
pagetree_init(struct pagetree *pt)
{
    memset(pt, 0, sizeof(*pt));
}

int
pagetree_copy(const struct pagetree *src, struct pagetree *dst,
	      int share_pinned)
{
    memcpy(dst, src, sizeof(*dst));

    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	if (dst->pt_direct[i].page)
	    page_to_pageinfo(dst->pt_direct[i].page)->pi_ref++;

    for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	if (dst->pt_indirect[i].page)
	    page_to_pageinfo(dst->pt_indirect[i].page)->pi_ref++;

    if (!share_pinned) {
	for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++) {
	    if (dst->pt_direct[i].page &&
		page_to_pageinfo(dst->pt_direct[i].page)->pi_pin) {
		int r = pagetree_cow(&dst->pt_direct[i]);
		if (r < 0) {
		    pagetree_free(dst);
		    return r;
		}
	    }
	}

	for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	    if (dst->pt_indirect[i].page &&
		page_to_pageinfo(dst->pt_indirect[i].page)->pi_pin) {
		int r = pagetree_cow(&dst->pt_indirect[i]);
		if (r < 0) {
		    pagetree_free(dst);
		    return r;
		}
	    }
	}
    }

    return 0;
}

static void
pagetree_free_ent(pagetree_entry *ent)
{
    if (ent->page) {
	pagetree_decref(ent->page);
	ent->page = 0;
    }
}

void
pagetree_free(struct pagetree *pt)
{
    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	pagetree_free_ent(&pt->pt_direct[i]);

    for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	pagetree_free_ent(&pt->pt_indirect[i]);

    pagetree_init(pt);
}

static int __attribute__ ((warn_unused_result))
pagetree_get_entp_indirect(pagetree_entry *indir, uint64_t npage,
			   pagetree_entry **outp, struct pagetree_indirect_page **out_parent,
			   page_sharing_mode rw, int level, struct pagetree_indirect_page *parent)
{
    if (!SAFE_EQUAL(rw, page_shared_ro)) {
	int r = pagetree_cow(indir);
	if (r < 0)
	    return r;
    }

    if (indir->page == 0) {
	if (SAFE_EQUAL(rw, page_shared_ro)) {
	    *outp = 0;
	    return 0;
	}

	int r = page_alloc(&indir->page);
	if (r < 0)
	    return r;

	memset(indir->page, 0, PGSIZE);
	pagetree_incref(indir->page);
	page_to_pageinfo(indir->page)->pi_indir = 1;
    }

    struct pagetree_indirect_page *pip = indir->page;
    struct page_info *pip_pi = page_to_pageinfo(pip);
    if (pip_pi->pi_parent != parent)
	pip_pi->pi_parent = parent;

    if (level == 0) {
	*outp = &pip->pt_entry[npage];
	*out_parent = pip;
	return 0;
    }

    uint64_t n_pages_per_pip_entry = 1;
    for (int i = 0; i < level; i++)
	n_pages_per_pip_entry *= PAGETREE_ENTRIES_PER_PAGE;

    uint32_t next_slot = npage / n_pages_per_pip_entry;
    uint32_t next_page = npage % n_pages_per_pip_entry;

    assert(next_slot < PAGETREE_ENTRIES_PER_PAGE);
    return pagetree_get_entp_indirect(&pip->pt_entry[next_slot],
				      next_page, outp, out_parent, rw, level - 1, pip);
}

static int __attribute__ ((warn_unused_result))
pagetree_get_entp(struct pagetree *pt, uint64_t npage,
		  pagetree_entry **entp, struct pagetree_indirect_page **out_parent,
		  page_sharing_mode rw)
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
					      npage, entp, out_parent,
					      rw, i, 0);
	npage -= num_indirect_pages;
    }

    cprintf("pagetree_get_entp: %"PRIu64" leftover!\n", npage);
    return -E_NO_SPACE;
}

int
pagetree_get_page(struct pagetree *pt, uint64_t npage,
		  void **pagep, page_sharing_mode rw)
{
    pagetree_entry *ent;
    struct pagetree_indirect_page *parent = 0;
    int r = pagetree_get_entp(pt, npage, &ent, &parent, rw);
    if (r < 0)
	return r;

    void *page = ent ? ent->page : 0;
    if (SAFE_EQUAL(rw, page_shared_ro) || page == 0) {
	*pagep = page;
	return 0;
    }

    r = pagetree_cow(ent);
    if (r < 0)
	return r;

    struct page_info *pi = page_to_pageinfo(ent->page);
    if (pi->pi_parent != parent)
	pi->pi_parent = parent;

    *pagep = ent->page;
    return 0;
}

int
pagetree_put_page(struct pagetree *pt, uint64_t npage, void *page)
{
    pagetree_entry *ent;
    struct pagetree_indirect_page *parent = 0;
    int r = pagetree_get_entp(pt, npage, &ent, &parent, page_excl_dirty);
    if (r < 0)
	return r;

    assert(ent != 0);
    if (ent->page)
	pagetree_decref(ent->page);
    if (page) {
	pagetree_incref(page);
	page_to_pageinfo(page)->pi_parent = parent;
    }
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

void
pagetree_incpin(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (pi->pi_ref != 1)
	panic("pagetree_incpin: shared page -- refcount %d", pi->pi_ref);
    ++pi->pi_pin;
    if (pi->pi_parent)
	pagetree_incpin(pi->pi_parent);
}

void
pagetree_decpin(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (pi->pi_ref != 1)
	panic("pagetree_decpin: shared page -- refcount %d", pi->pi_ref);
    if (pi->pi_pin == 0)
	panic("pagetree_decpin: not pinned");
    --pi->pi_pin;
    if (pi->pi_parent)
	pagetree_decpin(pi->pi_parent);
}
