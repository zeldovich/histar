#include <kern/segment.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <kern/pageinfo.h>
#include <kern/as.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>
#include <inc/safeint.h>

const struct Address_space *cur_as;
const struct Pagemap *cur_pgmap;

enum { as_debug = 0 };
enum { as_courtesy_pages = 8 };

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

int
as_copy(const struct Address_space *as, const struct Label *l, struct Address_space **asp)
{
    struct Address_space *nas;
    int r = as_alloc(l, &nas);
    if (r < 0)
	return r;

    r = kobject_copy_pages(&as->as_ko, &nas->as_ko);
    if (r < 0)
	return r;

    nas->as_utrap_entry = as->as_utrap_entry;
    nas->as_utrap_stack_base = as->as_utrap_stack_base;
    nas->as_utrap_stack_top = as->as_utrap_stack_top;

    *asp = nas;
    return 0;
}

void
as_invalidate(const struct Address_space *as_const)
{
    struct Address_space *as = &kobject_dirty(&as_const->as_ko)->as;

    if (as_debug)
	cprintf("as_invalidate: %s\n", as->as_ko.ko_name);

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
	       uint64_t smi, page_sharing_mode rw)
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
			      npage, (void **) &p, page_excl_dirty);
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
    int of = 0;
    uint64_t npages = (nent + N_USEGMAP_PER_PAGE - 1) / N_USEGMAP_PER_PAGE;
    uint64_t nbytes = safe_mul64(&of, npages, PGSIZE);
    if (of)
	return -E_INVAL;

    return kobject_set_nbytes(&as->as_ko, nbytes);
}

int
as_to_user(const struct Address_space *as, struct u_address_space *uas)
{
    int r = check_user_access(uas, sizeof(*uas), SEGMAP_WRITE);
    if (r < 0)
	return r;

    uint64_t size = uas->size;
    struct u_segment_mapping *ents = uas->ents;
    int overflow = 0;
    r = check_user_access(ents,
			  safe_mul64(&overflow, sizeof(*ents), size),
			  SEGMAP_WRITE);
    if (r < 0)
	return r;

    if (overflow)
	return -E_INVAL;

    uint64_t nent = 0;
    for (uint64_t i = 0; i < as_nents(as); i++) {
	const struct u_segment_mapping *usm;
	r = as_get_usegmap(as, &usm, i, page_shared_ro);
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
    uas->trap_handler = (void *) as->as_utrap_entry;
    uas->trap_stack_base = (void *) as->as_utrap_stack_base;
    uas->trap_stack_top = (void *) as->as_utrap_stack_top;
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
    int overflow = 0;
    r = check_user_access(ents,
			  safe_mul64(&overflow, sizeof(*ents), nent),
			  SEGMAP_WRITE);
    if (r < 0)
	return r;

    if (overflow)
	return -E_INVAL;

    // XXX Shrinking AS'es is a little tricky, so we don't do it for now
    if (nent > as_nents(as)) {
	r = as_resize(as, nent);
	if (r < 0)
	    return r;
    }

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct u_segment_mapping *usm;
	r = as_get_usegmap(as, (const struct u_segment_mapping **) &usm,
			   i, page_excl_dirty);
	if (r < 0)
	    goto out;

	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    goto out;

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

    r = 0;
    as->as_utrap_entry = (uintptr_t) uas->trap_handler;
    as->as_utrap_stack_base = (uintptr_t) uas->trap_stack_base;
    as->as_utrap_stack_top = (uintptr_t) uas->trap_stack_top;
out:
    as_invalidate(as);
    return r;
}

int
as_get_uslot(struct Address_space *as, struct u_segment_mapping *usm)
{
    int r = check_user_access(usm, sizeof(*usm), SEGMAP_WRITE);
    if (r < 0)
	return r;

    const struct u_segment_mapping *cur;
    r = as_get_usegmap(as, &cur, usm->kslot, page_shared_ro);
    if (r < 0)
	return r;

    memcpy(usm, cur, sizeof(*usm));
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
    r = as_get_usegmap(as, (const struct u_segment_mapping **) &usm,
		       slot, page_excl_dirty);
    if (r < 0)
	return r;

    struct segment_mapping *sm;
    r = as_get_segmap(as, &sm, slot);
    if (r < 0)
	return r;

    if (usm->segment.object    == usm_copy.segment.object &&
	usm->segment.container == usm_copy.segment.container &&
	usm->start_page        == usm_copy.start_page &&
	usm->kslot             == usm_copy.kslot &&
	usm->flags             == usm_copy.flags &&
	usm->va                == usm_copy.va &&
	usm->num_pages         <= usm_copy.num_pages &&
	!(usm->flags & SEGMAP_REVERSE_PAGES))
    {
	// Can skip invalidation -- simply growing the mapping.
    } else {
	// Invalidate any pre-existing mappings first..
	assert(!sm->sm_sg || as == sm->sm_as);
	as_invalidate_sm(sm);
    }

    *usm = usm_copy;

    return 0;
}

void
as_swapin(struct Address_space *as)
{
    as->as_pgmap = 0;
    pagetree_init(&as->as_segmap_pt);
}

void
as_swapout(struct Address_space *as)
{
    // In case we're the current AS, make sure the page table we're about
    // to free isn't the one being used by the CPU.
    if (cur_pgmap == as->as_pgmap)
	as_switch(0);

    if (as->as_pgmap) {
	assert(0 == page_map_traverse(as->as_pgmap, 0, (void **) ULIM,
				      0, &as_arch_page_invalidate_cb,
				      as->as_pgmap));
	page_map_free(as->as_pgmap);
    }

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	int r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    continue;

	if (sm->sm_sg) {
	    LIST_REMOVE(sm, sm_link);
	    kobject_unpin_page(&sm->sm_sg->sg_ko);
	}
    }

    pagetree_free(&as->as_segmap_pt, 0);
}

int
as_gc(struct Address_space *as)
{
    as_swapout(as);
    return 0;
}

static uint64_t
as_va_to_segment_page(const struct u_segment_mapping *usm, void *va)
{
    void *usm_va = ROUNDDOWN(usm->va, PGSIZE);
    assert(va >= usm_va && va < usm_va + usm->num_pages * PGSIZE);
    uint64_t mapping_page = ((uintptr_t) (va - usm_va)) / PGSIZE;
    if (usm->flags & SEGMAP_REVERSE_PAGES)
	mapping_page = (usm->num_pages - mapping_page - 1);
    return usm->start_page + mapping_page;
}

static int
as_pmap_fill_segment(const struct Address_space *as,
		     const struct Segment *sg,
		     struct segment_mapping *sm,
		     const struct u_segment_mapping *usm,
		     void *need_va, uint32_t usmflags)
{
    if (usm->num_pages == 0)
	return -E_INVAL;

    int of = 0;
    need_va = ROUNDDOWN(need_va, PGSIZE);

    void *usm_first = ROUNDDOWN(usm->va, PGSIZE);
    void *usm_last = (void *)
	safe_addptr(&of, (uintptr_t) usm_first,
		    safe_mulptr(&of, usm->num_pages - 1, PGSIZE));
    if (of || PGOFF(usm->va))
	return -E_INVAL;
    assert(need_va >= usm_first && need_va <= usm_last);

    void *map_first = usm_first;
    void *map_last  = usm_last;

    if (need_va - map_first > as_courtesy_pages * PGSIZE)
	map_first = need_va - as_courtesy_pages * PGSIZE;
    if (map_last - need_va > as_courtesy_pages * PGSIZE)
	map_last = need_va + as_courtesy_pages * PGSIZE;

    if (as_debug)
	cprintf("as_pmap_fill_segment: va %p start %"PRIu64" num %"PRIu64" need %p: mapping %p--%p\n",
		usm->va, usm->start_page, usm->num_pages, need_va, map_first, map_last);

    if (sm->sm_sg) {
	LIST_REMOVE(sm, sm_link);
	kobject_unpin_page(&sm->sm_sg->sg_ko);
	sm->sm_sg = 0;
    }

    int r = 0;
    for (void *va = map_first; va <= map_last; va += PGSIZE) {
	void *pp = 0;
	uint64_t segpage = as_va_to_segment_page(usm, va);

	if (va != need_va && (usmflags & SEGMAP_WRITE)) {
	    /*
	     * If this is a courtesy writable mapping, and there's a refcount
	     * on this page, defer doing the actual copy-on-write until user
	     * hits this exact page, because it's quite costly.
	     */
	    if (segpage >= kobject_npages(&sg->sg_ko))
		continue;

	    struct kobject *sg_ko = kobject_ephemeral_dirty(&sg->sg_ko);
	    r = pagetree_get_page(&sg_ko->ko_pt, segpage, &pp, page_shared_ro);
	    if (r < 0 || !pp || page_to_pageinfo(pp)->pi_ref > 1)
		continue;
	}

	r = kobject_get_page(&sg->sg_ko, segpage, &pp,
			     (usmflags & SEGMAP_WRITE) ? page_excl_dirty_later
						       : page_shared_ro);
	if (r < 0) {
	    if (va != need_va)
		continue;
	    goto err;
	}

	r = as_arch_putpage(as->as_pgmap, va, pp, usmflags);
	if (r < 0) {
	    if (va != need_va)
		continue;
	    goto err;
	}

	struct page_info *pi = page_to_pageinfo(pp);
	pi->pi_seg = sg->sg_ko.ko_id;
	pi->pi_segpg = segpage;
    }

    sm->sm_as = as;
    sm->sm_sg = sg;
    if (usmflags & SEGMAP_WRITE)
	sm->sm_rw_mappings = 1;

    struct Segment *msg = &kobject_dirty(&sg->sg_ko)->sg;
    LIST_INSERT_HEAD(&msg->sg_segmap_list, sm, sm_link);
    kobject_pin_page(&sg->sg_ko);
    return 0;

err:
    if (r != -E_RESTART && as_debug)
	cprintf("as_pmap_fill_segment: %s\n", e2s(r));

    assert(page_map_traverse(as->as_pgmap, usm_first, usm_last,
			     0, &as_arch_page_invalidate_cb,
			     as->as_pgmap) == 0);
    return r;
}

static int
as_pmap_fill(const struct Address_space *as, void *va, uint32_t reqflags)
{
    for (uint64_t i = 0; i < as_nents(as); i++) {
	const struct u_segment_mapping *usm;
	int r = as_get_usegmap(as, &usm, i, page_shared_ro);
	if (r < 0)
	    return r;

	uint64_t flags = usm->flags;
	if (!(flags & SEGMAP_READ))
	    continue;
	if ((flags & reqflags) != reqflags)
	    continue;
 
	int of = 0;
	uint64_t npages = usm->num_pages;
	void *va_start = ROUNDDOWN(usm->va, PGSIZE);
	void *va_end = (void *) (uintptr_t)
	    safe_addptr(&of, (uintptr_t) va_start,
			safe_mulptr(&of, npages, PGSIZE));
	if (of || va < va_start || va >= va_end)
	    continue;

	struct cobj_ref seg_ref = usm->segment;
	const struct kobject *ko;

 retry_ro:
	r = cobj_get(seg_ref, kobj_segment, &ko,
		     (flags & SEGMAP_WRITE) ? iflow_rw : iflow_read);
	if (r < 0) {
	    /*
	     * If this is a writable segment that we can't write to,
	     * and the faulting instruction is not a write, try to
	     * map the segment as read-only as a fall-back.
	     */
	    if (r == -E_LABEL && (flags & SEGMAP_WRITE) &&
		!(reqflags & SEGMAP_WRITE))
	    {
		flags &= ~SEGMAP_WRITE;
		goto retry_ro;
	    }

	    return r;
	}

	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    return r;

	if (as_debug)
	    cprintf("as_pmap_fill: fault %p base %p\n", va, va_start);

	const struct Segment *sg = &ko->sg;
	sm->sm_as_slot = i;
	sm->sm_ct_id = seg_ref.container;
	return as_pmap_fill_segment(as, sg, sm, usm, va, flags);
    }

    return -E_NOT_FOUND;
}

int
as_pagefault(const struct Address_space *as, void *va, uint32_t reqflags)
{
    if (!as->as_pgmap) {
	struct Address_space *mas = &kobject_dirty(&as->as_ko)->as;

	int r = page_map_alloc(&mas->as_pgmap);
	if (r < 0)
	    return r;

	mas->as_pgmap_tls_id = cur_thread->th_sg;
	mas->as_pgmap_label_id = cur_thread->th_ko.ko_label[kolabel_contaminate];
    }

    if (as_debug)
	cprintf("as_pagefault: as %s va %p\n", as->as_ko.ko_name, va);

    return as_pmap_fill(as, va, reqflags);
}

static void
as_switch_invalidate(const struct Address_space *as,
		     int invalidate_tls, int check_label)
{
    int r;

    if (!invalidate_tls && !check_label)
	return;

    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    goto err;

	if (!sm->sm_sg)
	    continue;

	const struct u_segment_mapping *usm;
	r = as_get_usegmap(as, &usm, i, page_shared_ro);
	if (r < 0)
	    goto err;

	int do_invalidate = 0;

	if (invalidate_tls)
	    if (usm->segment.object == kobject_id_thread_sg)
		do_invalidate = 1;

	if (check_label && !do_invalidate) {
	    const struct kobject *ko;
	    if (cobj_get(usm->segment, kobj_segment, &ko,
		    (usm->flags & SEGMAP_WRITE) ? iflow_rw : iflow_read) < 0)
		do_invalidate = 1;
	}

	if (do_invalidate) {
	    if (as_debug)
		cprintf("as_switch_invalidate: calling as_invalidate_sm\n");
	    assert(as == sm->sm_as);
	    as_invalidate_sm(sm);
	}
    }

    return;

err:
    cprintf("as_switch_invalidate: fallback to full invalidation: %s\n", e2s(r));
    as_invalidate(as);
}

void
as_switch(const struct Address_space *as)
{
    // In case we have thread-specific kobjects cached here..
    if (as && cur_thread) {
	kobject_id_t cur_tls = cur_thread->th_sg;
	kobject_id_t cur_lbl = cur_thread->th_ko.ko_label[kolabel_contaminate];
	as_switch_invalidate(as,
			     as->as_pgmap_tls_id != cur_tls,
			     as->as_pgmap_label_id != cur_lbl);

	struct Address_space *das = &kobject_dirty(&as->as_ko)->as;
	das->as_pgmap_tls_id = cur_tls;
	das->as_pgmap_label_id = cur_lbl;
    }

    struct Pagemap *new_pgmap = as ? as->as_pgmap : 0;

    cur_as = as;
    pmap_set_current(new_pgmap);
}

void
as_collect_dirty_sm(struct segment_mapping *sm)
{
    if (!sm->sm_rw_mappings)
	return;

    const struct u_segment_mapping *usm;
    assert(as_get_usegmap(sm->sm_as, &usm, sm->sm_as_slot, page_shared_ro) == 0);
    assert(page_map_traverse(sm->sm_as->as_pgmap,
			     usm->va,
			     usm->va + (usm->num_pages - 1) * PGSIZE,
			     0, &as_arch_collect_dirty_bits,
			     sm->sm_as->as_pgmap) == 0);
}

void
as_map_ro_sm(struct segment_mapping *sm)
{
    if (!sm->sm_rw_mappings)
	return;

    const struct u_segment_mapping *usm;
    assert(as_get_usegmap(sm->sm_as, &usm, sm->sm_as_slot, page_shared_ro) == 0);

    void *map_first = usm->va;
    void *map_last = usm->va + (usm->num_pages - 1) * PGSIZE;
    assert(page_map_traverse(sm->sm_as->as_pgmap, map_first, map_last,
			     0, &as_arch_page_map_ro_cb,
			     sm->sm_as->as_pgmap) == 0);
    sm->sm_rw_mappings = 0;
}

void
as_invalidate_sm(struct segment_mapping *sm)
{
    if (!sm->sm_sg)
	return;

    if (as_debug)
	cprintf("as_invalidate_sm\n");

    const struct u_segment_mapping *usm;
    int r = as_get_usegmap(sm->sm_as, &usm, sm->sm_as_slot, page_shared_ro);
    if (r < 0)
	panic("as_invalidate_sm: cannot fetch usegmap: %s", e2s(r));

    void *map_first = usm->va;
    void *map_last = usm->va + (usm->num_pages - 1) * PGSIZE;
    assert(page_map_traverse(sm->sm_as->as_pgmap, map_first, map_last, 0,
			     &as_arch_page_invalidate_cb,
			     sm->sm_as->as_pgmap) == 0);

    LIST_REMOVE(sm, sm_link);
    kobject_unpin_page(&sm->sm_sg->sg_ko);
    sm->sm_sg = 0;
    sm->sm_rw_mappings = 0;
    return;
}

int
as_invert_mapped(const struct Address_space *as, void *addr,
		 uint64_t *segp, uint64_t *offsetp)
{
    if (as->as_pgmap) {
	ptent_t *pte;
	int r = pgdir_walk(as->as_pgmap, addr, 0, &pte);
	if (r >= 0 && *pte) {
	    struct page_info *pi = page_to_pageinfo(pa2kva(PTE_ADDR(*pte)));
	    if (pi->pi_pin) {	/* page not shared by multiple segments */
		*segp = pi->pi_seg;
		*offsetp = (pi->pi_segpg * PGSIZE) + PGOFF(addr);
		return 0;
	    }
	}
    }

    int found = 0;
    for (uint64_t i = 0; i < as_nents(as); i++) {
	struct segment_mapping *sm;
	int r = as_get_segmap(as, &sm, i);
	if (r < 0)
	    return r;

	if (!sm->sm_sg)
	    continue;

	const struct u_segment_mapping *usm;
	r = as_get_usegmap(as, &usm, i, page_shared_ro);
	if (r < 0)
	    return r;

	void *va_start = ROUNDDOWN(usm->va, PGSIZE);
	void *va_end = (char*) va_start + usm->num_pages * PGSIZE;
	if (addr < va_start || addr >= va_end)
	    continue;

	uint64_t npage = as_va_to_segment_page(usm, addr);
	*segp = usm->segment.object;
	*offsetp = npage * PGSIZE + PGOFF(addr);
	found++;
    }

    return found == 1 ? 0 : -E_NOT_FOUND;
}
