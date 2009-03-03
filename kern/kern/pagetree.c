#include <machine/types.h>
#include <machine/pmap.h>
#include <kern/pagetree.h>
#include <kern/pageinfo.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <inc/error.h>

static void pagetree_decref(void *p);

static void
pagetree_link_entry(struct pagetree_entry *ep)
{
    LIST_INSERT_HEAD(&page_to_pageinfo(ep->page)->pi_plist, ep, link);
}

static void
pagetree_set_entry(struct pagetree_entry *ep, void *p)
{
    if (ep->page)
	LIST_REMOVE(ep, link);
    if (p)
	LIST_INSERT_HEAD(&page_to_pageinfo(p)->pi_plist, ep, link);
    ep->page = p;
}

static void
pagetree_free_page(void *p)
{
    struct page_info *ptp = page_to_pageinfo(p);
    assert(ptp->pi_ref == 0);
    assert(ptp->pi_write_shared_ref == 0);
    assert(ptp->pi_hw_write_pin == 0);
    assert(ptp->pi_hw_read_pin == 0);

    if (ptp->pi_indir) {
	struct pagetree_indirect_page *pip = p;
	for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++) {
	    if (pip->pt_entry[i].page) {
		pagetree_decref(pip->pt_entry[i].page);
		pagetree_set_entry(&pip->pt_entry[i], p);
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

    for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++) {
	if (pdst->pt_entry[i].page) {
	    pagetree_incref(pdst->pt_entry[i].page);
	    pagetree_link_entry(&pdst->pt_entry[i]);
	}

	pdst->pt_entry[i].parent = (void *) (((uintptr_t) pdst) | 1);
    }

    for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++) {
	if (pdst->pt_entry[i].page &&
	    page_to_pageinfo(pdst->pt_entry[i].page)->pi_hw_write_pin) {
	    int r = pagetree_cow(&pdst->pt_entry[i]);
	    if (r < 0)
		return r;
	}
    }

    assert(page_to_pageinfo(src)->pi_indir);
    page_to_pageinfo(dst)->pi_indir = 1;
    return 0;
}

static void
pagetree_invalidate_ro_segments(void *pg)
{
    struct pagetree_entry *pe, *next;
    for (pe = LIST_FIRST(&page_to_pageinfo(pg)->pi_plist); pe; pe = next) {
	next = LIST_NEXT(pe, link);
	uintptr_t parentip = (uintptr_t) pe->parent;
	if (parentip & 1) {
	    /* Points to an indirect page */
	    void *parent = (void *) (uintptr_t) (parentip & UINT64(~0));
	    pagetree_invalidate_ro_segments(parent);
	    continue;
	}

	if (parentip == 0) {
	    /* Dead-end: could be a snapshot pagetree */
	    continue;
	}

	const struct kobject *ko = (const void *) parentip;
	if (ko->hdr.ko_type != kobj_segment)
	    panic("pagetree_invalidate_ro_segments: odd type %d\n",
		  ko->hdr.ko_type);

	segment_invalidate(&ko->sg, 0);
    }
}

static int __attribute__ ((warn_unused_result))
pagetree_cow(pagetree_entry *ent)
{
    if (!ent->page)
	return 0;

    struct page_info *ptp = page_to_pageinfo(ent->page);
    assert(ptp->pi_ref > 0);

    if (ptp->pi_ref <= 1 + ptp->pi_write_shared_ref)
	return 0;

    if (page_to_pageinfo(ent->page)->pi_hw_read_pin) {
	/*
	 * There are some read-only mappings of this page, and they
	 * might or might not be in the same pagetree.  We need to
	 * invalidate all of them.
	 */
	assert(!page_to_pageinfo(ent->page)->pi_indir);
	pagetree_invalidate_ro_segments(ent->page);
    }

    void *copy;
    int r = page_alloc(&copy);
    if (r < 0)
	return r;

    assert(page_to_pageinfo(copy)->pi_ref == 0);
    assert(page_to_pageinfo(copy)->pi_write_shared_ref == 0);
    assert(page_to_pageinfo(copy)->pi_hw_write_pin == 0);
    assert(page_to_pageinfo(copy)->pi_hw_read_pin == 0);
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
    pagetree_set_entry(ent, copy);
    return 1;
}

void
pagetree_init(struct pagetree *pt, void *kobjptr)
{
    memset(pt, 0, sizeof(*pt));
    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	pt->pt_direct[i].parent = kobjptr;
    for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	pt->pt_indirect[i].parent = kobjptr;
}

int
pagetree_copy(const struct pagetree *src, struct pagetree *dst,
	      void *kobjptr, int *share_pinned)
{
    memcpy(dst, src, sizeof(*dst));

    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++) {
	if (dst->pt_direct[i].page) {
	    pagetree_incref(dst->pt_direct[i].page);
	    pagetree_link_entry(&dst->pt_direct[i]);
	}
	dst->pt_direct[i].parent = kobjptr;
    }

    for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	if (dst->pt_indirect[i].page) {
	    pagetree_incref(dst->pt_indirect[i].page);
	    pagetree_link_entry(&dst->pt_indirect[i]);
	}
	dst->pt_indirect[i].parent = kobjptr;
    }

    if (share_pinned)
	*share_pinned = 0;

    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++) {
	if (dst->pt_direct[i].page &&
	    page_to_pageinfo(dst->pt_direct[i].page)->pi_hw_write_pin)
	{
	    if (share_pinned) {
		*share_pinned = 1;
		goto sharepin;
	    }

	    int r = pagetree_cow(&dst->pt_direct[i]);
	    if (r < 0) {
		pagetree_free(dst, 0);
		return r;
	    }
	}
    }

    for (int i = 0; i < PAGETREE_INDIRECTS; i++) {
	if (dst->pt_indirect[i].page &&
	    page_to_pageinfo(dst->pt_indirect[i].page)->pi_hw_write_pin)
	{
	    if (share_pinned) {
		*share_pinned = 1;
		goto sharepin;
	    }

	    int r = pagetree_cow(&dst->pt_indirect[i]);
	    if (r < 0) {
		pagetree_free(dst, 0);
		return r;
	    }
	}
    }

 sharepin:
    if (share_pinned && *share_pinned) {
	for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	    if (dst->pt_direct[i].page)
		page_to_pageinfo(dst->pt_direct[i].page)->pi_write_shared_ref++;

	for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	    if (dst->pt_indirect[i].page)
		page_to_pageinfo(dst->pt_indirect[i].page)->pi_write_shared_ref++;
    }

    return 0;
}

static void
pagetree_free_ent(pagetree_entry *ent)
{
    if (ent->page) {
	pagetree_decref(ent->page);
	pagetree_set_entry(ent, 0);
    }
}

void
pagetree_free(struct pagetree *pt, int was_share_pinned)
{
    if (was_share_pinned) {
	for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	    if (pt->pt_direct[i].page)
		page_to_pageinfo(pt->pt_direct[i].page)->pi_write_shared_ref--;

	for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	    if (pt->pt_indirect[i].page)
		page_to_pageinfo(pt->pt_indirect[i].page)->pi_write_shared_ref--;
    }

    for (int i = 0; i < PAGETREE_DIRECT_PAGES; i++)
	pagetree_free_ent(&pt->pt_direct[i]);

    for (int i = 0; i < PAGETREE_INDIRECTS; i++)
	pagetree_free_ent(&pt->pt_indirect[i]);

    pagetree_init(pt, 0);
}

static int __attribute__ ((warn_unused_result))
pagetree_get_entp_indirect(pagetree_entry *indir, uint64_t npage,
			   pagetree_entry **outp,
			   struct pagetree_indirect_page **out_parent,
			   page_sharing_mode rw, int level,
			   struct pagetree_indirect_page *parent)
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

	void *p;
	int r = page_alloc(&p);
	if (r < 0)
	    return r;

	memset(p, 0, PGSIZE);
	pagetree_incref(p);
	pagetree_set_entry(indir, p);
	page_to_pageinfo(indir->page)->pi_indir = 1;

	struct pagetree_indirect_page *pip = p;
	for (uint32_t i = 0; i < PAGETREE_ENTRIES_PER_PAGE; i++)
	    pip->pt_entry[i].parent = (void *) (((uintptr_t) pip) | 1);
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
				      next_page, outp, out_parent,
				      rw, level - 1, pip);
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
    return r;
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
    pagetree_set_entry(ent, page);

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
pagetree_incpin_write(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (!pi)
	return;

    if (pi->pi_ref != 1 + pi->pi_write_shared_ref)
	panic("pagetree_incpin_write: shared page -- refcount %d", pi->pi_ref);
    ++pi->pi_hw_write_pin;
    if (pi->pi_hw_write_pin == 0)
	panic("pagetree_incpin_write: overflow\n");
    if (pi->pi_parent)
	pagetree_incpin_write(pi->pi_parent);
}

void
pagetree_decpin_write(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (!pi)
	return;

    if (pi->pi_ref != 1 + pi->pi_write_shared_ref)
	panic("pagetree_decpin_write: shared page, refcount %d", pi->pi_ref);
    if (pi->pi_hw_write_pin == 0)
	panic("pagetree_decpin_write: not pinned");
    --pi->pi_hw_write_pin;
    if (pi->pi_parent) {
	if (pi->pi_ref != 1)
	    panic("pagetree_decpin_write: shared subpage, refcount %d", pi->pi_ref);
	pagetree_decpin_write(pi->pi_parent);
    }
}

void
pagetree_incpin_read(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (!pi)
	return;

    ++pi->pi_hw_read_pin;
    if (pi->pi_hw_read_pin == 0)
	panic("pagetree_incpin_read: overflow\n");
}

void
pagetree_decpin_read(void *p)
{
    struct page_info *pi = page_to_pageinfo(p);
    if (!pi)
	return;

    if (pi->pi_hw_read_pin == 0)
	panic("pagetree_decpin_read: not pinned\n");
    --pi->pi_hw_read_pin;
}
