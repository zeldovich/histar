#include <machine/x86.h>
#include <machine/pmap.h>
#include <kern/lib.h>
#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/pageinfo.h>
#include <inc/error.h>
#include <inc/safeint.h>
#include <inc/intmacro.h>

static void
page_map_free_level(struct Pagemap *pgmap, int pmlevel)
{
    // Skip the kernel half of the address space
    int maxi = (pmlevel == NPTLVLS ? NPTENTRIES/2 : NPTENTRIES);
    int i;

    for (i = 0; i < maxi; i++) {
	ptent_t ptent = pgmap->pm_ent[i];
	if (!(ptent & PTE_P))
	    continue;
	if (pmlevel > 0) {
	    struct Pagemap *pm = (struct Pagemap *) pa2kva(PTE_ADDR(ptent));
	    page_map_free_level(pm, pmlevel - 1);
	}
    }

    page_free(pgmap);
}

void
page_map_free(struct Pagemap *pgmap)
{
    page_map_free_level(pgmap, NPTLVLS);
}

static int
page_map_traverse_internal(struct Pagemap *pgmap, int pmlevel,
			   const void *first, const void *last,
			   int create,
			   page_map_traverse_cb cb, const void *arg,
			   void *va_base)
{
    int r;
    assert(pmlevel >= 0 && pmlevel <= NPTLVLS);

    uint32_t first_idx = PDX(pmlevel, first);
    uint32_t last_idx  = PDX(pmlevel, last);

    for (uint64_t idx = first_idx; idx <= last_idx; idx++) {
	ptent_t *pm_entp = &pgmap->pm_ent[idx];
	ptent_t pm_ent = *pm_entp;

	void *ent_va = va_base + (idx << PDSHIFT(pmlevel));

	if (pmlevel == 0) {
	    cb(arg, pm_entp, ent_va);
	    continue;
	}

	if (pmlevel == 1 && (pm_ent & PTE_PS)) {
	    cb(arg, pm_entp, ent_va);
	    continue;
	}

	if (!(pm_ent & PTE_P)) {
	    if (!create)
		continue;

	    void *p;
	    if ((r = page_alloc(&p)) < 0)
		return r;

	    memset(p, 0, PGSIZE);
	    pm_ent = kva2pa(p) | PTE_P | PTE_U | PTE_W;
	    *pm_entp = pm_ent;
	}

	struct Pagemap *pm_next = (struct Pagemap *) pa2kva(PTE_ADDR(pm_ent));
	const void *first_next = (idx == first_idx) ? first : 0;
	const void *last_next  = (idx == last_idx)  ? last  : (const void *) (uintptr_t) UINT64(~0);
	r = page_map_traverse_internal(pm_next, pmlevel - 1,
				       first_next, last_next,
				       create, cb, arg, ent_va);
	if (r < 0)
	    return r;
    }

    return 0;
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
    if (last >= (const void *) ULIM)
	last = (const void *) ULIM - PGSIZE;
    return page_map_traverse_internal(pgmap, NPTLVLS, first, last,
				      create, cb, arg, 0);
}

static void
pgdir_walk_cb(const void *arg, ptent_t *ptep, void *va)
{
    ptent_t **pte_store = (ptent_t **) arg;
    *pte_store = ptep;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
    *pte_store = 0;
    int r = page_map_traverse(pgmap, va, va, create, &pgdir_walk_cb, pte_store);
    if (r < 0)
	return r;
    if (create && !*pte_store)
	return -E_INVAL;
    return 0;
}

static void *
page_lookup(struct Pagemap *pgmap, void *va, ptent_t **pte_store)
{
    if ((uintptr_t) va >= ULIM)
	panic("page_lookup: va %p over ULIM", va);

    ptent_t *ptep;
    int r = pgdir_walk(pgmap, va, 0, &ptep);
    if (r < 0)
	panic("pgdir_walk(%p, create=0) failed: %d", va, r);

    if (pte_store)
	*pte_store = ptep;

    if (ptep == 0 || !(*ptep & PTE_P))
	return 0;

    return pa2kva(PTE_ADDR(*ptep));
}

int
check_user_access(const void *ptr, uint64_t nbytes, uint32_t reqflags)
{
    assert(cur_thread);
    if (!cur_as) {
	int r = thread_load_as(cur_thread);
	if (r < 0)
	    return r;

	as_switch(cur_thread->th_as);
	assert(cur_as);
    }

    ptent_t pte_flags = PTE_P | PTE_U;
    if (reqflags & SEGMAP_WRITE)
	pte_flags |= PTE_W;

    int aspf = 0;
    if (nbytes > 0) {
	int overflow = 0;
	uintptr_t iptr = (uintptr_t) ptr;
	uintptr_t start = ROUNDDOWN(iptr, PGSIZE);
	uintptr_t end = ROUNDUP(safe_addptr(&overflow, iptr, nbytes), PGSIZE);

	if (end <= start || overflow)
	    return -E_INVAL;

	for (uintptr_t va = start; va < end; va += PGSIZE) {
	    if (va >= ULIM)
		return -E_INVAL;

	    ptent_t *ptep;
	    if (cur_as->as_pgmap &&
		page_lookup(cur_as->as_pgmap, (void *) va, &ptep) &&
		(*ptep & pte_flags) == pte_flags)
		continue;

	    aspf = 1;
	    int r = as_pagefault(cur_as, (void *) va, reqflags);
	    if (r < 0)
		return r;
	}
    }

    // Flush any stale TLB entries that might have arisen from as_pagefault()
    if (aspf)
	as_switch(cur_as);

    return 0;
}

ppn_t
pa2ppn(physaddr_t pa)
{
    ppn_t pn = pa >> PGSHIFT;
    if (pn > global_npages)
	panic("pa2ppn: pa 0x%lx out of range, npages %"PRIu64,
	      (unsigned long) pa, global_npages);
    return pn;
}

physaddr_t
ppn2pa(ppn_t pn)
{
    if (pn > global_npages)
	panic("ppn2pa: ppn %lx out of range, npages %"PRIu64,
	      (unsigned long) pn, global_npages);
    return (pn << PGSHIFT);
}

/*
 * Page table entry management.
 */

enum { pmap_invlpg_max = 4 };
static uint32_t cur_pgmap_invlpg_count;
static void *cur_pgmap_invlpg_addrs[pmap_invlpg_max];

static void
pmap_queue_invlpg(const struct Pagemap *pgmap, void *addr)
{
    if (cur_pgmap != pgmap)
	return;

    if (cur_pgmap_invlpg_count >= pmap_invlpg_max) {
	cur_pgmap_invlpg_count = pmap_invlpg_max + 1;
	return;
    }

    if (cur_pgmap_invlpg_count > 0 &&
	cur_pgmap_invlpg_addrs[cur_pgmap_invlpg_count - 1] == addr)
	return;

    cur_pgmap_invlpg_addrs[cur_pgmap_invlpg_count++] = addr;
}

void
pmap_set_current(struct Pagemap *new_pgmap)
{
    int flush_tlb = 0;
    if (cur_pgmap != new_pgmap || cur_pgmap_invlpg_count > pmap_invlpg_max) {
	flush_tlb = 1;
    } else {
	for (uint32_t i = 0; i < cur_pgmap_invlpg_count; i++)
	    invlpg(cur_pgmap_invlpg_addrs[i]);
    }

    cur_pgmap = new_pgmap;
    cur_pgmap_invlpg_count = 0;

    if (flush_tlb)
	pmap_set_current_arch(new_pgmap);
}

void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
    const struct Pagemap *pgmap = arg;
    uint64_t pte = *ptep;
    if (!(pte & PTE_P) || !(pte & PTE_D))
	return;

    struct page_info *pi = page_to_pageinfo(pa2kva(PTE_ADDR(pte)));
    pi->pi_dirty = 1;
    *ptep &= ~PTE_D;
    pmap_queue_invlpg(pgmap, va);
}

void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    const struct Pagemap *pgmap = arg;
    as_arch_collect_dirty_bits(arg, ptep, va);

    uint64_t pte = *ptep;
    if ((pte & PTE_P)) {
	if (pte & PTE_W)
	    pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep = 0;
	pmap_queue_invlpg(pgmap, va);
    }
}

void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
    const struct Pagemap *pgmap = arg;
    as_arch_collect_dirty_bits(arg, ptep, va);

    uint64_t pte = *ptep;
    if ((pte & PTE_P) && (pte & PTE_W)) {
	pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep &= ~PTE_W;
	pmap_queue_invlpg(pgmap, va);
    }
}

int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
    uint64_t ptflags = PTE_P | PTE_U | PTE_NX;
    if ((flags & SEGMAP_WRITE))
	ptflags |= PTE_W;
    if ((flags & SEGMAP_EXEC))
	ptflags &= ~PTE_NX;

    ptent_t *ptep;
    int r = pgdir_walk(pgmap, va, 1, &ptep);
    if (r < 0)
	return r;

    as_arch_page_invalidate_cb(pgmap, ptep, va);
    *ptep = kva2pa(pp) | ptflags;
    if ((ptflags & PTE_W))
	pagetree_incpin(pp);

    pmap_queue_invlpg(pgmap, va);
    return 0;
}
