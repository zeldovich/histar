#include <machine/x86.h>
#include <machine/as.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <inc/error.h>

int
as_alloc(const struct Label *l, struct Address_space **asp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_address_space, l, &ko);
    if (r < 0)
	return r;

    struct Address_space *as = &ko->u.as;
    as_swapin(as);

    *asp = as;
    return 0;
}

void
as_invalidate(const struct Address_space *as_const)
{
    struct Address_space *as = &kobject_dirty(&as_const->as_ko)->u.as;

    as_swapout(as);
    as_swapin(as);
}

static uint64_t
as_nents(const struct Address_space *as)
{
    return as->as_ko.ko_npages * N_USEGMAP_PER_PAGE;
}

static int
as_get_usegmap(const struct Address_space *as,
	       const struct u_segment_mapping ** smp,
	       uint64_t smi, page_rw_mode rw)
{
    uint64_t npage = smi / N_USEGMAP_PER_PAGE;
    uint64_t pagei = smi % N_USEGMAP_PER_PAGE;

    struct u_segment_mapping *p;
    int r = kobject_get_page(&as->as_ko, npage, (void **) &p, rw);
    if (r < 0)
	return r;

    *smp = &p[pagei];
    return 0;
}

static int
as_get_segmap(const struct Address_space *as,
	      struct segment_mapping **smp, uint64_t smi)
{
    uint64_t npage = smi / N_SEGMAP_PER_PAGE;
    uint64_t pagei = smi % N_SEGMAP_PER_PAGE;

    struct segment_mapping *p;
    int r = pagetree_get_page((struct pagetree *) &as->as_segmap_pt,
			      npage, (void **) &p, page_rw);
    if (r < 0)
	return r;

    if (p == 0) {
	r = page_alloc((void **) &p);
	if (r < 0)
	    return r;

	memset(p, 0, PGSIZE);
	r = pagetree_put_page((struct pagetree *) &as->as_segmap_pt, npage, p);
	if (r < 0) {
	    page_free(p);
	    return r;
	}
    }

    *smp = &p[pagei];
    return 0;
}

static int
as_resize(struct Address_space *as, uint64_t nent)
{
    uint64_t npages = (nent + N_USEGMAP_PER_PAGE - 1) / N_USEGMAP_PER_PAGE;
    return kobject_set_npages(&as->as_ko, npages);
}

int
as_to_user(const struct Address_space *as, struct u_address_space *uas)
{
    int r = page_user_incore((void**) &uas, sizeof(*uas));
    if (r < 0)
	return r;

    uint64_t size = uas->size;
    struct u_segment_mapping *ents = uas->ents;
    r = page_user_incore((void**) &ents, sizeof(*ents) * size);
    if (r < 0)
	return r;

    uint64_t nent = 0;
    for (uint64_t i = 0; i < as_nents(as); i++) {
	const struct u_segment_mapping *usm;
	r = as_get_usegmap(as, &usm, i, page_ro);
	if (r < 0)
	    return r;

	if (usm->flags == 0)
	    continue;

	if (nent >= size)
	    return -E_NO_SPACE;

	ents[nent] = *usm;
	nent++;
    }

    uas->nent = nent;
    return 0;
}

int
as_from_user(struct Address_space *as, struct u_address_space *uas)
{
    int r = page_user_incore((void**) &uas, sizeof(*uas));
    if (r < 0)
	return r;

    uint64_t nent = uas->nent;
    const struct u_segment_mapping *ents = uas->ents;
    r = page_user_incore((void**) &ents, sizeof(*ents) * nent);
    if (r < 0)
	return r;

    // XXX Shrinking AS'es is a little tricky, so we don't do it for now
    if (nent > as_nents(as)) {
	r = as_resize(as, nent);
	if (r < 0)
	    return r;
    }

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct u_segment_mapping *usm;
	r = as_get_usegmap(as, (const struct u_segment_mapping **) &usm,
			   i, page_rw);
	if (r < 0)
	    return r;

	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    return r;

	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_unpin_page(&sm->sm_sg->sg_ko);
	}

	memset(sm, 0, sizeof(*sm));
	memset(usm, 0, sizeof(*usm));
	if (i < nent)
	    *usm = ents[i];
    }

    as_invalidate(as);
    return 0;
}

void
as_swapin(struct Address_space *as)
{
    as->as_pgmap = &bootpml4;
    pagetree_init(&as->as_segmap_pt);
}

void
as_swapout(struct Address_space *as)
{
    if (as->as_pgmap && as->as_pgmap != &bootpml4)
	page_map_free(as->as_pgmap);

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	assert(0 == as_get_segmap(as, &sm, i));
	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_unpin_page(&sm->sm_sg->sg_ko);
	}
    }

    pagetree_free(&as->as_segmap_pt);
}

int
as_gc(struct Address_space *as)
{
    as_swapout(as);
    return 0;
}

static int
as_pmap_fill_segment(const struct Address_space *as,
		     const struct Segment *sg,
		     struct segment_mapping *sm,
		     const struct u_segment_mapping *usm)
{
    struct Pagemap *pgmap = as->as_pgmap;

    char *cva = (char *) usm->va;
    if (PGOFF(cva))
	return -E_INVAL;

    uint64_t start_page = usm->start_page;
    uint64_t num_pages = usm->num_pages;
    uint64_t flags = usm->flags;

    for (uint64_t i = start_page; i < start_page + num_pages; i++) {
	void *pp;
	int r = kobject_get_page(&sg->sg_ko, i, &pp,
				 (flags & SEGMAP_WRITE) ? page_rw : page_ro);

	if (((uint64_t) cva) >= ULIM)
	    r = -E_INVAL;

	uint64_t ptflags = PTE_NX;
	if ((flags & SEGMAP_WRITE))
	    ptflags |= PTE_W;
	if ((flags & SEGMAP_EXEC))
	    ptflags &= ~PTE_NX;

	if (r == 0) {
	    page_remove(pgmap, cva);
	    r = page_insert(pgmap, pp, cva, PTE_U | ptflags);
	}
	if (r < 0) {
	    cva = (char *) usm->va;
	    uint64_t cleanup_end = i;
	    for (i = start_page; i < cleanup_end; i++) {
		page_remove(pgmap, cva);
		cva += PGSIZE;
	    }
	    return r;
	}

	cva += PGSIZE;
    }

    if (sm->sm_sg != sg) {
	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_unpin_page(&sm->sm_sg->sg_ko);
	}

	sm->sm_as = as;
	sm->sm_sg = sg;

	struct Segment *msg = &kobject_dirty(&sg->sg_ko)->u.sg;
	LIST_INSERT_HEAD(&msg->sg_segmap_list, sm, sm_link);
	kobject_pin_page(&sg->sg_ko);
    }

    return 0;
}

static int
as_pmap_fill(struct Address_space *as, void *va)
{
    for (uint64_t i = 0; i < as_nents(as); i++) {
	const struct u_segment_mapping *usm;
	int r = as_get_usegmap(as, &usm, i, page_ro);
	if (r < 0)
	    return r;

	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    return r;

	uint64_t flags = usm->flags;
	if (flags == 0)
	    continue;

	uint64_t npages = usm->num_pages;
	void *va_start = usm->va;
	void *va_end = (char*) va_start + npages * PGSIZE;
	if (va < va_start || va >= va_end)
	    continue;

	struct cobj_ref seg_ref = usm->segment;
	const struct kobject *ko;
	r = cobj_get(seg_ref, kobj_segment, &ko,
		     (flags & SEGMAP_WRITE) ? iflow_rw : iflow_read);
	if (r < 0)
	    return r;

	const struct Segment *sg = &ko->u.sg;
	sm->sm_as_slot = i;
	return as_pmap_fill_segment(as, sg, sm, usm);
    }

    return -E_INVAL;
}

int
as_pagefault(struct Address_space *as, void *va)
{
    if (as->as_pgmap == &bootpml4) {
	int r = page_map_alloc(&as->as_pgmap);
	if (r < 0)
	    return r;

	as->as_pgmap_tid = cur_thread->th_ko.ko_id;
    }

    int r = as_pmap_fill(as, va);
    if (r == -E_RESTART)
	return r;

    if (r < 0) {
	cprintf("as_pagefault(%ld: %s, %p): %s\n",
			      as->as_ko.ko_id,
			      as->as_ko.ko_name[0] ? &as->as_ko.ko_name[0]
						   : "unnamed",
			      va, e2s(r));
	return r;
    }

    return 0;
}

void
as_switch(const struct Address_space *as)
{
    // In case we have thread-specific kobjects cached here..
    if (as && cur_thread && as->as_pgmap_tid != cur_thread->th_ko.ko_id)
	as_invalidate(as);

    struct Pagemap *pgmap = as ? as->as_pgmap : &bootpml4;
    lcr3(kva2pa(pgmap));
}
