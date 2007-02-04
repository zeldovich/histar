#include <kern/segment.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <kern/pageinfo.h>
#include <kern/as.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/safeint.h>

const struct Address_space *cur_as;

enum { as_invlpg_max = 4 };
static uint32_t cur_as_invlpg_count;
static void *cur_as_invlpg_addrs[as_invlpg_max];

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
    uint64_t nbytes = safe_mul(&of, npages, PGSIZE);
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
			  safe_mul(&overflow, sizeof(*ents), size),
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
			  safe_mul(&overflow, sizeof(*ents), nent),
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
	usm->num_pages         <= usm_copy.num_pages)
    {
	// Can skip invalidation -- simply growing the mapping.
    } else {
	// Invalidate any pre-existing mappings first..
	as_invalidate_sm(sm);
    }

    *usm = usm_copy;

    return 0;
}

static void
as_queue_invlpg(const struct Address_space *as, void *addr)
{
    if (cur_as != as)
	return;

    if (cur_as_invlpg_count >= as_invlpg_max) {
	cur_as_invlpg_count = as_invlpg_max + 1;
	return;
    }

    if (cur_as_invlpg_count > 0 &&
	cur_as_invlpg_addrs[cur_as_invlpg_count - 1] == addr)
	return;

    cur_as_invlpg_addrs[cur_as_invlpg_count++] = addr;
}

static void
as_collect_dirty_bits(const void *arg, uint64_t *ptep, void *va)
{
    const struct Address_space *as = arg;
    uint64_t pte = *ptep;
    if (!(pte & PTE_P) || !(pte & PTE_D))
	return;

    struct page_info *pi = page_to_pageinfo(pa2kva(PTE_ADDR(pte)));
    pi->pi_dirty = 1;
    *ptep &= ~PTE_D;
    as_queue_invlpg(as, va);
}

static void
as_page_invalidate_cb(const void *arg, uint64_t *ptep, void *va)
{
    const struct Address_space *as = arg;
    as_collect_dirty_bits(arg, ptep, va);

    uint64_t pte = *ptep;
    if ((pte & PTE_P)) {
	if ((pte & PTE_W))
	    pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep = 0;
	as_queue_invlpg(as, va);
    }
}

static void
as_page_map_ro_cb(const void *arg, uint64_t *ptep, void *va)
{
    const struct Address_space *as = arg;
    as_collect_dirty_bits(arg, ptep, va);

    uint64_t pte = *ptep;
    if ((pte & PTE_P) && (pte & PTE_W)) {
	pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep &= ~PTE_W;
	as_queue_invlpg(as, va);
    }
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
    if (cur_as && cur_as->as_ko.ko_id == as->as_ko.ko_id)
	as_switch(0);

    if (as->as_pgmap) {
	assert(0 == page_map_traverse(as->as_pgmap, 0, (void **) ULIM,
				      0, &as_page_invalidate_cb, as));
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
		     uint64_t need_page_in_map)
{
    if (usm->num_pages == 0)
	return -E_INVAL;

    // Argument is relative to mapping base, not segment base -- shift it
    uint64_t need_page_in_seg = need_page_in_map + usm->start_page;
    int of = 0;

    uint64_t map_start_page =
	MAX(usm->start_page,
	    need_page_in_seg > as_courtesy_pages
		? need_page_in_seg - as_courtesy_pages : 0);

    uint64_t map_end_page =
	MIN(usm->start_page + usm->num_pages - 1,
	    safe_add(&of, need_page_in_seg, as_courtesy_pages));

    void *usm_last_va = (void *) (uintptr_t)
	safe_add(&of, (uintptr_t) usm->va,
		 safe_mul(&of, usm->num_pages - 1, PGSIZE));
    if (of || PGOFF(usm->va))
	return -E_INVAL;

    if (as_debug)
	cprintf("as_pmap_fill_segment: start %"PRIu64" num %"PRIu64" need %"PRIu64": mapping %"PRIu64"--%"PRIu64"\n",
		usm->start_page, usm->num_pages, need_page_in_seg, map_start_page, map_end_page);

    if (sm->sm_sg) {
	LIST_REMOVE(sm, sm_link);
	kobject_unpin_page(&sm->sm_sg->sg_ko);
	sm->sm_sg = 0;
    }

    int r = 0;
    for (uint64_t i = map_start_page; i <= map_end_page; i++) {
	void *pp = 0;
	r = kobject_get_page(&sg->sg_ko, i, &pp,
			     (usm->flags & SEGMAP_WRITE) ? page_excl_dirty_later
							 : page_shared_ro);
	if (r < 0)
	    goto err;

	uint64_t ptflags = PTE_P | PTE_U | PTE_NX;
	if ((usm->flags & SEGMAP_WRITE))
	    ptflags |= PTE_W;
	if ((usm->flags & SEGMAP_EXEC))
	    ptflags &= ~PTE_NX;

	char *cva = (char *) usm->va + (i - usm->start_page) * PGSIZE;
	uint64_t *ptep;
	r = pgdir_walk(as->as_pgmap, cva, 1, &ptep);
	if (r < 0)
	    goto err;

	as_page_invalidate_cb(as, ptep, cva);
	*ptep = kva2pa(pp) | ptflags;
	if ((ptflags & PTE_W))
	    pagetree_incpin(pp);

	as_queue_invlpg(as, cva);
    }

    sm->sm_as = as;
    sm->sm_sg = sg;
    if ((usm->flags & SEGMAP_WRITE))
	sm->sm_rw_mappings = 1;

    struct Segment *msg = &kobject_dirty(&sg->sg_ko)->sg;
    LIST_INSERT_HEAD(&msg->sg_segmap_list, sm, sm_link);
    kobject_pin_page(&sg->sg_ko);
    return 0;

err:
    if (r != -E_RESTART)
	cprintf("as_pmap_fill_segment: %s\n", e2s(r));

    assert(page_map_traverse(as->as_pgmap, usm->va, usm_last_va,
			     0, &as_page_invalidate_cb, as) == 0);
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
	    safe_add(&of, (uintptr_t) va_start,
		     safe_mul(&of, npages, PGSIZE));
	if (of || va < va_start || va >= va_end)
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

	uint64_t fault_page = (ROUNDDOWN(va, PGSIZE) - va_start) / PGSIZE;

	if (as_debug)
	    cprintf("as_pmap_fill: fault %p base %p page# %"PRIu64"\n",
		    va, va_start, fault_page);

	const struct Segment *sg = &ko->sg;
	sm->sm_as_slot = i;
	return as_pmap_fill_segment(as, sg, sm, usm, fault_page);
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

	mas->as_pgmap_tid = cur_thread->th_ko.ko_id;
    }

    if (as_debug)
	cprintf("as_pagefault: as %s va %p\n", as->as_ko.ko_name, va);

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

    int flush_tlb = 0;
    if (cur_as_invlpg_count > as_invlpg_max) {
	flush_tlb = 1;
    } else {
	for (uint32_t i = 0; i < cur_as_invlpg_count; i++)
	    pmap_tlb_invlpg(cur_as_invlpg_addrs[i]);
    }

    cur_as = as;
    cur_as_invlpg_count = 0;
    pmap_set_current(as ? as->as_pgmap : 0, flush_tlb);
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
			     0, &as_collect_dirty_bits, sm->sm_as) == 0);
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
			     0, &as_page_map_ro_cb, sm->sm_as) == 0);
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
    assert(0 == as_get_usegmap(sm->sm_as, &usm, sm->sm_as_slot, page_shared_ro));

    void *map_first = usm->va;
    void *map_last = usm->va + (usm->num_pages - 1) * PGSIZE;
    assert(page_map_traverse(sm->sm_as->as_pgmap, map_first, map_last, 0,
			     &as_page_invalidate_cb, sm->sm_as) == 0);

    LIST_REMOVE(sm, sm_link);
    kobject_unpin_page(&sm->sm_sg->sg_ko);
    sm->sm_sg = 0;
    sm->sm_rw_mappings = 0;
    return;
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
	r = as_get_usegmap(as, &usm, i, page_shared_ro);
	if (r < 0)
	    goto err;

	const struct kobject *ko;
	if ((invalidate_tls && usm->segment.object == kobject_id_thread_sg) ||
	    cobj_get(usm->segment, kobj_segment, &ko,
		     (usm->flags & SEGMAP_WRITE) ? iflow_rw : iflow_read) < 0)
	{
	    if (as_debug)
		cprintf("as_invalidate_label: calling as_invalidate_sm\n");
	    as_invalidate_sm(sm);
	}
    }

    return;

err:
    cprintf("as_invalidate_label: fallback to full invalidation: %s\n", e2s(r));
    as_invalidate(as);
}

int
as_invert_mapped(const struct Address_space *as, void *addr,
		 kobject_id_t *seg_idp, uint64_t *offsetp)
{
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

	uint64_t npages = usm->num_pages;
	void *va_start = ROUNDDOWN(usm->va, PGSIZE);
	void *va_end = (char*) va_start + npages * PGSIZE;
	if (addr < va_start || addr >= va_end)
	    continue;

	*seg_idp = usm->segment.object;
	*offsetp = usm->start_page * PGSIZE +
		   ((uintptr_t) addr) - ((uintptr_t) va_start);
	found++;
    }

    return found == 1 ? 0 : -E_NOT_FOUND;
}
