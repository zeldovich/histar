#include <machine/x86.h>
#include <machine/as.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <inc/error.h>

const struct Address_space *cur_as;

enum { as_debug = 0 };
enum { as_invlpg_max = 1 };

int
as_alloc(const struct Label *l, struct Address_space **asp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_address_space, l, &ko);
    if (r < 0)
	return r;

    struct Address_space *as = &ko->as;
    as_swapin(as);

    *asp = as;
    return 0;
}

void
as_invalidate(const struct Address_space *as_const)
{
    struct Address_space *as = &kobject_dirty(&as_const->as_ko)->as;

    if (as_debug)
	cprintf("as_invalidate\n");

    as_swapout(as);
    as_swapin(as);
}

static uint64_t
as_nents(const struct Address_space *as)
{
    return kobject_npages(&as->as_ko) * N_USEGMAP_PER_PAGE;
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
    return kobject_set_nbytes(&as->as_ko, npages * PGSIZE);
}

int
as_to_user(const struct Address_space *as, struct u_address_space *uas)
{
    int r = check_user_access(uas, sizeof(*uas), SEGMAP_WRITE);
    if (r < 0)
	return r;

    uint64_t size = uas->size;
    struct u_segment_mapping *ents = uas->ents;
    r = check_user_access(ents, sizeof(*ents) * size, SEGMAP_WRITE);
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
    int r = check_user_access(uas, sizeof(*uas), 0);
    if (r < 0)
	return r;

    uint64_t nent = uas->nent;
    struct u_segment_mapping *ents = uas->ents;
    r = check_user_access(ents, sizeof(*ents) * nent, SEGMAP_WRITE);
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
	if (i < nent) {
	    ents[i].kslot = i;
	    *usm = ents[i];
	}
    }

    as_invalidate(as);
    return 0;
}

int
as_set_uslot(struct Address_space *as, struct u_segment_mapping *usm_new)
{
    int r = check_user_access(usm_new, sizeof(*usm_new), 0);
    if (r < 0)
	return r;

    struct u_segment_mapping usm_copy = *usm_new;
    uint64_t slot = usm_copy.kslot;

    struct u_segment_mapping *usm;
    r = as_get_usegmap(as, (const struct u_segment_mapping **) &usm, slot, page_rw);
    if (r < 0)
	return r;

    struct segment_mapping *sm;
    r = as_get_segmap(as, &sm, slot);
    if (r < 0)
	return r;

    // Invalidate any pre-existing mappings first..
    as_invalidate_sm(sm);

    *usm = usm_copy;

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
    // In case we're the current AS, make sure the page table we're about
    // to free isn't the one being used by the CPU.
    as_switch(0);

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
		     const struct u_segment_mapping *usm,
		     int invalidate)
{
    struct Pagemap *pgmap = as->as_pgmap;

    char *cva = (char *) usm->va;
    if (PGOFF(cva))
	return -E_INVAL;

    uint64_t start_page = usm->start_page;
    uint64_t num_pages = usm->num_pages;
    uint64_t flags = usm->flags;

    uint64_t i;
    int r = 0;

    if (num_pages > as_invlpg_max)
	kobject_ephemeral_dirty(&as->as_ko)->as.as_dirty_tlb = 1;

    for (i = start_page; i < start_page + num_pages; i++) {
	if (((uint64_t) cva) >= ULIM) {
	    r = -E_INVAL;
	    goto err;
	}

	void *pp = 0;
	if (!invalidate) {
	    r = kobject_get_page(&sg->sg_ko, i, &pp,
				 (flags & SEGMAP_WRITE) ? page_rw : page_ro);
	    if (r < 0)
		goto err;
	}

	uint64_t ptflags = PTE_P | PTE_U | PTE_NX;
	if ((flags & SEGMAP_WRITE))
	    ptflags |= PTE_W;
	if ((flags & SEGMAP_EXEC))
	    ptflags &= ~PTE_NX;

	uint64_t *ptep;
	if (invalidate) {
	    r = pgdir_walk(pgmap, cva, 0, &ptep);
	    if (r < 0)
		goto err;

	    if (ptep)
		*ptep = 0;
	} else {
	    r = pgdir_walk(pgmap, cva, 1, &ptep);
	    if (r < 0)
		goto err;

	    *ptep = kva2pa(pp) | ptflags;
	}

	if (num_pages <= as_invlpg_max)
	    tlb_invalidate(pgmap, cva);

	cva += PGSIZE;
    }

    if (sm->sm_sg) {
	LIST_REMOVE(sm, sm_link);
	kobject_unpin_page(&sm->sm_sg->sg_ko);
	sm->sm_sg = 0;
    }

    if (!invalidate) {
	sm->sm_as = as;
	sm->sm_sg = sg;

	struct Segment *msg = &kobject_dirty(&sg->sg_ko)->sg;
	LIST_INSERT_HEAD(&msg->sg_segmap_list, sm, sm_link);
	kobject_pin_page(&sg->sg_ko);
    }

    return 0;

err:
    if (r != -E_RESTART)
	cprintf("as_pmap_fill_segment: %s\n", e2s(r));

    cva = (char *) usm->va;
    uint64_t cleanup_end = i;
    for (i = start_page; i <= cleanup_end; i++) {
	if ((uint64_t) cva < ULIM)
	    page_remove(pgmap, cva);
	cva += PGSIZE;
    }
    return r;
}

static int
as_pmap_fill(const struct Address_space *as, void *va, uint32_t reqflags)
{
    for (uint64_t i = 0; i < as_nents(as); i++) {
	const struct u_segment_mapping *usm;
	int r = as_get_usegmap(as, &usm, i, page_ro);
	if (r < 0)
	    return r;

	uint64_t flags = usm->flags;
	if (flags == 0)
	    continue;
	if ((flags & reqflags) != reqflags)
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

	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    return r;

	const struct Segment *sg = &ko->sg;
	sm->sm_as_slot = i;
	return as_pmap_fill_segment(as, sg, sm, usm, 0);
    }

    return -E_NOT_FOUND;
}

int
as_pagefault(const struct Address_space *as, void *va, uint32_t reqflags)
{
    if (as->as_pgmap == &bootpml4) {
	struct Address_space *mas = &kobject_dirty(&as->as_ko)->as;

	int r = page_map_alloc(&mas->as_pgmap);
	if (r < 0)
	    return r;

	mas->as_pgmap_tid = cur_thread->th_ko.ko_id;
    }

    return as_pmap_fill(as, va, reqflags);
}

void
as_switch(const struct Address_space *as)
{
    // In case we have thread-specific kobjects cached here..
    if (as && cur_thread && as->as_pgmap_tid != cur_thread->th_ko.ko_id) {
	as_invalidate_label(as, 1);
	kobject_dirty(&as->as_ko)->as.as_pgmap_tid = cur_thread->th_ko.ko_id;
    }

    cur_as = as;

    int dirty_tlb = as ? as->as_dirty_tlb : 0;
    if (dirty_tlb)
	kobject_ephemeral_dirty(&as->as_ko)->as.as_dirty_tlb = 0;

    struct Pagemap *pgmap = as ? as->as_pgmap : &bootpml4;
    uint64_t new_cr3 = kva2pa(pgmap);
    uint64_t cur_cr3 = rcr3();
    if (cur_cr3 != new_cr3 || dirty_tlb)
	lcr3(new_cr3);
}

void
as_invalidate_sm(struct segment_mapping *sm)
{
    if (!sm->sm_sg)
	return;

    if (as_debug)
	cprintf("as_invalidate_sm\n");

    const struct u_segment_mapping *usm;
    int r = as_get_usegmap(sm->sm_as, &usm, sm->sm_as_slot, page_ro);
    if (r < 0)
	goto err;

    r = as_pmap_fill_segment(sm->sm_as, sm->sm_sg, sm, usm, 1);
    if (r < 0)
	goto err;

    return;

err:
    cprintf("as_invalidate_sm: fallback to full invalidation: %s\n", e2s(r));
    as_invalidate(sm->sm_as);
}

void
as_invalidate_label(const struct Address_space *as, int invalidate_tls)
{
    int r;

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    goto err;

	if (!sm->sm_sg)
	    continue;

	const struct u_segment_mapping *usm;
	r = as_get_usegmap(as, &usm, i, page_ro);
	if (r < 0)
	    goto err;

	const struct kobject *ko;
	if (usm->segment.object != kobject_id_thread_sg &&
	    cobj_get(usm->segment, kobj_segment, &ko,
		     (usm->flags & SEGMAP_WRITE) ? iflow_rw : iflow_read) == 0)
	    continue;

	as_invalidate_sm(sm);
    }

    return;

err:
    cprintf("as_invalidate_label: fallback to full invalidation: %s\n", e2s(r));
    as_invalidate(as);
}
