#include <machine/x86.h>
#include <machine/as.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <inc/error.h>

int
as_alloc(struct Label *l, struct Address_space **asp)
{
    struct Address_space *as;
    int r = kobject_alloc(kobj_address_space, l, (struct kobject **) &as);
    if (r < 0)
	return r;

    static_assert(sizeof(*as) <= sizeof(struct kobject_buf));

    memset(&as->as_segmap, 0, sizeof(as->as_segmap));
    as_swapin(as);

    *asp = as;
    return 0;
}

void
as_invalidate(struct Address_space *as)
{
    as_swapout(as);
    as_swapin(as);
}

static int
as_nents(struct Address_space *as)
{
    return N_SEGMAP_DIRECT + as->as_ko.ko_npages * N_SEGMAP_PER_PAGE;
}

static int
as_get_segmap(struct Address_space *as, struct segment_mapping **smp,
	      uint64_t smi, kobj_rw_mode rw)
{
    if (smi < N_SEGMAP_DIRECT) {
	*smp = &as->as_segmap[smi];
	return 0;
    }
    smi -= N_SEGMAP_DIRECT;

    for (uint64_t i = 0; i < as->as_ko.ko_npages; i++) {
	if (smi < N_SEGMAP_PER_PAGE) {
	    struct segment_mapping *p;
	    int r = kobject_get_page(&as->as_ko, i, (void **) &p, rw);
	    if (r < 0)
		return r;

	    *smp = &p[smi];
	    return 0;
	}
	smi -= N_SEGMAP_PER_PAGE;
    }

    return -E_RANGE;
}

static int
as_resize(struct Address_space *as, uint64_t nent)
{
    uint64_t npages = 0;

    if (nent > N_SEGMAP_DIRECT) {
	nent -= N_SEGMAP_DIRECT;
	npages = (nent + N_SEGMAP_PER_PAGE - 1) / N_SEGMAP_PER_PAGE;
    }

    return kobject_set_npages(&as->as_ko, npages);
}

int
as_to_user(struct Address_space *as, struct u_address_space *uas)
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
	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i, kobj_ro);
	if (r < 0)
	    return r;

	if (sm->sm_usm.flags == 0)
	    continue;

	if (nent >= size)
	    return -E_NO_SPACE;

	ents[nent] = sm->sm_usm;
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
    struct u_segment_mapping *ents = uas->ents;
    r = page_user_incore((void**) &ents, sizeof(*ents) * nent);
    if (r < 0)
	return r;

    // Shrinking AS'es is a little tricky, so we don't do it for now
    if (nent > as_nents(as)) {
	r = as_resize(as, nent);
	if (r < 0)
	    return r;
    }

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i, kobj_rw);
	if (r < 0)
	    return r;

	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_decpin(&sm->sm_sg->sg_ko);
	}

	memset(sm, 0, sizeof(*sm));
	if (i < nent)
	    sm->sm_usm = ents[i];
    }

    as_invalidate(as);
    return 0;
}

void
as_swapin(struct Address_space *as)
{
    as->as_pgmap = &bootpml4;

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	assert(0 == as_get_segmap(as, &sm, i, kobj_rw));
	sm->sm_sg = 0;
    }
}

void
as_swapout(struct Address_space *as)
{
    if (as->as_pgmap && as->as_pgmap != &bootpml4)
	page_map_free(as->as_pgmap);

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	assert(0 == as_get_segmap(as, &sm, i, kobj_rw));
	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_decpin(&sm->sm_sg->sg_ko);
	}
    }
}

int
as_gc(struct Address_space *as)
{
    as_swapout(as);
    return 0;
}

static int
as_pmap_fill_segment(struct Address_space *as,
		     struct Segment *sg,
		     struct segment_mapping *sm,
		     bool force_ro)
{
    struct Pagemap *pgmap = as->as_pgmap;

    char *cva = sm->sm_usm.va;
    if (PGOFF(cva))
	return -E_INVAL;

    uint64_t start_page = sm->sm_usm.start_page;
    uint64_t num_pages = sm->sm_usm.num_pages;
    uint64_t flags = sm->sm_usm.flags;

    if (force_ro)
	flags &= ~SEGMAP_WRITE;

    for (int64_t i = start_page; i < start_page + num_pages; i++) {
	void *pp;
	int r = kobject_get_page(&sg->sg_ko, i, &pp,
				 (flags & SEGMAP_WRITE) ? kobj_rw : kobj_ro);

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
	    cva = sm->sm_usm.va;
	    int64_t cleanup_end = i;
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
	    kobject_decpin(&sm->sm_sg->sg_ko);
	}

	sm->sm_as = as;
	sm->sm_sg = sg;

	LIST_INSERT_HEAD(&sg->sg_segmap_list, sm, sm_link);
	kobject_incpin(&sg->sg_ko);
    }

    return 0;
}

static int
as_pmap_fill(struct Address_space *as, void *va)
{
    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *segmap;
	int r = as_get_segmap(as, &segmap, i, kobj_ro);
	if (r < 0)
	    return r;

	uint64_t flags = segmap->sm_usm.flags;
	if (flags == 0)
	    continue;

	uint64_t npages = segmap->sm_usm.num_pages;
	void *va_start = segmap->sm_usm.va;
	void *va_end = va_start + npages * PGSIZE;
	if (va < va_start || va >= va_end)
	    continue;

	// Now grab it as read-write
	r = as_get_segmap(as, &segmap, i, kobj_rw);
	if (r < 0)
	    return r;

	struct cobj_ref seg_ref = segmap->sm_usm.segment;
	struct Segment *sg;
	r = cobj_get(seg_ref, kobj_segment, (struct kobject **)&sg,
		     (flags & SEGMAP_WRITE) ? iflow_rw : iflow_read);
	if (r < 0)
	    return r;

	return as_pmap_fill_segment(as, sg, segmap, 0);
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
as_switch(struct Address_space *as)
{
    struct Pagemap *pgmap = as ? as->as_pgmap : &bootpml4;
    lcr3(kva2pa(pgmap));
}

void
as_segmap_snapshot(struct Address_space *as, struct segment_mapping *sm)
{
    assert(0 == as_pmap_fill_segment(as, sm->sm_sg, sm, 1));
}
