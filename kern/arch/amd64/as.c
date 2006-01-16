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

    memset(&as->as_segmap, 0, sizeof(as->as_segmap));
    as_swapin(as);

    *asp = as;
    return 0;
}

static void
as_invalidate(struct Address_space *as)
{
    as_swapout(as);
    as_swapin(as);
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
    for (uint64_t i = 0; i < NSEGMAP; i++) {
	if (as->as_segmap[i].sm_usm.flags == 0)
	    continue;

	if (nent >= size)
	    return -E_NO_SPACE;

	ents[nent] = as->as_segmap[i].sm_usm;
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

    if (nent > NSEGMAP)
	return -E_NO_SPACE;

    memset(&as->as_segmap, 0, sizeof(as->as_segmap));
    for (uint64_t i = 0; i < nent; i++)
	as->as_segmap[i].sm_usm = ents[i];

    as_invalidate(as);
    return 0;
}

void
as_swapin(struct Address_space *as)
{
    as->as_pgmap = &bootpml4;

    for (int i = 0; i < NSEGMAP; i++)
	as->as_segmap[i].sm_sg = 0;
}

void
as_swapout(struct Address_space *as)
{
    if (as->as_pgmap && as->as_pgmap != &bootpml4)
	page_map_free(as->as_pgmap);

    for (int i = 0; i < NSEGMAP; i++) {
	if (as->as_segmap[i].sm_sg) {
	    LIST_REMOVE(&as->as_segmap[i], sm_link);
	    kobject_decpin(&as->as_segmap[i].sm_sg->sg_ko);
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
		     struct segment_mapping *sm)
{
    struct Pagemap *pgmap = as->as_pgmap;

    char *cva = sm->sm_usm.va;
    if (PGOFF(cva))
	return -E_INVAL;

    uint64_t start_page = sm->sm_usm.start_page;
    uint64_t num_pages = sm->sm_usm.num_pages;
    uint64_t flags = sm->sm_usm.flags;

    for (int64_t i = start_page; i < start_page + num_pages; i++) {
	void *pp;
	int r = kobject_get_page(&sg->sg_ko, i, &pp);

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
	    for (; i >= start_page; i--) {
		page_remove(pgmap, cva);
		cva -= PGSIZE;
	    }
	}

	cva += PGSIZE;
    }

    sm->sm_as = as;
    sm->sm_sg = sg;

    LIST_INSERT_HEAD(&sg->sg_segmap_list, sm, sm_link);
    kobject_incpin(&sg->sg_ko);

    return 0;
}

static int
as_pmap_fill(struct Address_space *as, void *va)
{
    for (int i = 0; i < NSEGMAP; i++) {
	struct segment_mapping *segmap = &as->as_segmap[i];

	uint64_t flags = segmap->sm_usm.flags;
	if (flags == 0)
	    continue;

	uint64_t npages = segmap->sm_usm.num_pages;
	void *va_start = segmap->sm_usm.va;
	void *va_end = va_start + npages * PGSIZE;
	if (va < va_start || va >= va_end)
	    continue;

	struct cobj_ref seg_ref = segmap->sm_usm.segment;
	struct Segment *sg;
	int r = cobj_get(seg_ref, kobj_segment, (struct kobject **)&sg,
			 (flags & SEGMAP_WRITE) ? iflow_rw : iflow_read);
	if (r < 0)
	    return r;

	return as_pmap_fill_segment(as, sg, segmap);
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
