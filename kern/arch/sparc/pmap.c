#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <machine/sparc-common.h>
#include <machine/page.h>
#include <inc/error.h>
#include <inc/safeint.h>

static ptent_t*
pmap_entp(uint32_t *pgmap, int pmlevel, uint32_t idx)
{
    if (pmlevel == 2)
	return &((struct Pagemap *)(pgmap))->pm1_ent[idx];
    return &((struct Pagemap2 *)(pgmap))->pm2_ent[idx];
}

static int
pmap_alloc_pmap2(struct Pagemap2fl *fl, struct Pagemap2 **pgmap)
{
    struct Pagemap2 *pm = LIST_FIRST(fl);
    if (pm) {
	LIST_REMOVE(pm, pm2_link);
	*pgmap = pm;
	return 0;
    } 

    void *p;
    int r = page_alloc(&p);
    if (r < 0)
	return r;
    memset(p, 0, PGSIZE);

    pm = (struct Pagemap2 *)p;
    *pgmap = pm;
    struct Pagemap2 *end = pm + (PGSIZE / sizeof(struct Pagemap2));
    for (pm = pm + 1; pm != end; pm++)
	LIST_INSERT_HEAD(fl, pm, pm2_link);

    return 0;
}

static void
pmap_free_pmap2(struct Pagemap2fl *fl, struct Pagemap2 *pgmap)
{
    LIST_INSERT_HEAD(fl, pgmap, pm2_link);
}

int
page_map_alloc(struct Pagemap **pm_store)
{
    void *pmap;
    int r = page_alloc(&pmap);
    if (r < 0)
	return r;

    memcpy(pmap, &bootpt, sizeof(bootpt));
    *pm_store = (struct Pagemap *) pmap;
    return 0;
}

static void
page_map_free_level(uint32_t *pgmap, int pmlevel, struct Pagemap2fl *fl)
{
    // Skip the kernel half of the address space
    int maxi = (pmlevel == NPTLVLS ? NPTENTRIES(pmlevel)/2 : NPTENTRIES(pmlevel));
    int i;

    for (i = 0; i < maxi; i++) {
	ptent_t ptent = *pmap_entp(pgmap, pmlevel, i);
	if (PT_ET(ptent) == PT_ET_NONE)
	    continue;
	if (pmlevel > 0) {
	    uint32_t *pm = pa2kva(PTD_ADDR(ptent));
	    page_map_free_level(pm, pmlevel - 1, fl);
	}
    }

    if (pmlevel == NPTLVLS)
	page_free(pgmap);
    else
	pmap_free_pmap2(fl, (struct Pagemap2 *)pgmap);
}

void
page_map_free(struct Pagemap *pgmap)
{
    page_map_free_level((uint32_t *)pgmap, NPTLVLS, &pgmap->fl);
    
    struct Pagemap2 *pm2;
    LIST_FOREACH(pm2, &pgmap->fl, pm2_link) {
	uint32_t addr = (uint32_t)pm2;
	if ((addr % PGSIZE) == 0)
	    page_free(pm2);
    }
}

static int
page_map_traverse_internal(uint32_t *pgmap, int pmlevel, struct Pagemap2fl *fl,
			   const void *first, const void *last,
			   int create,
			   page_map_traverse_cb cb, const void *arg,
			   void *va_base)
{
    int r;
    assert(pmlevel >= 0 && pmlevel <= NPTLVLS);

    uint32_t first_idx = PDX(pmlevel, first);
    uint32_t last_idx  = PDX(pmlevel, last);

    for (uint32_t idx = first_idx; idx <= last_idx; idx++) {
	ptent_t *pm_entp = pmap_entp(pgmap, pmlevel, idx);
	ptent_t pm_ent = *pm_entp;

	void *ent_va = va_base + (idx << PDSHIFT(pmlevel));

	if (pmlevel == 0) {
	    cb(arg, pm_entp, ent_va);
	    continue;
	}

	if ((pmlevel == 1 || pmlevel == 2) && (PT_ET(pm_ent) == PT_ET_PTE)) {
	    panic("XXX");
	    cb(arg, pm_entp, ent_va);
	    continue;
	}

	if (PT_ET(pm_ent) == PT_ET_NONE) {
	    if (!create)
		continue;

	    struct Pagemap2 *pgmap2 = 0;
	    if ((r = pmap_alloc_pmap2(fl, &pgmap2)) < 0)
		return r;

	    pm_ent = PTD_ENTRY(kva2pa(pgmap2));
	    *pm_entp = pm_ent;
	}

	uint32_t *pm_next = pa2kva(PTD_ADDR(pm_ent));
	const void *first_next = (idx == first_idx) ? first : 0;
	const void *last_next  = (idx == last_idx)  ? last  : (const void *) ~0;
	r = page_map_traverse_internal(pm_next, pmlevel - 1, fl,
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
    return page_map_traverse_internal((uint32_t *)pgmap, NPTLVLS, &pgmap->fl, 
				      first, last, create, cb, arg, 0);
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
    /* XXX LEON3 MMU missing probe!? */
    if ((uintptr_t) va >= ULIM)
	panic("page_lookup: va %p over ULIM", va);

    ptent_t *ptep;
    int r = pgdir_walk(pgmap, va, 0, &ptep);
    if (r < 0)
	panic("pgdir_walk(%p, create=0) failed: %d", va, r);

    if (pte_store)
	*pte_store = ptep;

    if (ptep == 0 || PT_ET(*ptep) == PT_ET_NONE)
	return 0;

    return pa2kva(PTE_ADDR(*ptep));
}

int
check_user_access2(const void *ptr, uint64_t nbytes,
		   uint32_t reqflags, int alignbytes)
{
    assert(cur_thread);
    if (!cur_as) {
	int r = thread_load_as(cur_thread);
	if (r < 0)
	    return r;

	as_switch(cur_thread->th_as);
	assert(cur_as);
    }

    ptent_t pte_flags = 0;
    if (reqflags & SEGMAP_WRITE)
	pte_flags |= PTE_ACC_W;
    
    int aspf = 0;
    if (nbytes > 0) {
	int overflow = 0;
	uintptr_t iptr = (uintptr_t) ptr;
	uintptr_t start = ROUNDDOWN(iptr, PGSIZE);
	uintptr_t end = ROUNDUP(safe_addptr(&overflow, iptr, nbytes), PGSIZE);

	if (iptr & (alignbytes - 1))
	    return -E_INVAL;

	if (end <= start || overflow)
	    return -E_INVAL;

	for (uintptr_t va = start; va < end; va += PGSIZE) {
	    if (va >= ULIM)
		return -E_INVAL;

	    ptent_t *ptep;
	    if (cur_as->as_pgmap &&
		page_lookup(cur_as->as_pgmap, (void *) va, &ptep) &&
		(PTE_ACC(*ptep) & pte_flags) == pte_flags)
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

void
pmap_set_current(struct Pagemap *pm)
{
    if (!pm)
	pm = &bootpt;

    uint32_t pa = (ctxptr_t)kva2pa(pm);
    ctxptr_t ptd = (pa >> 4) & PTD_PTP_MASK;
    ptd |= PT_ET_PTD;
    bootct.ct_ent[0] = ptd;
    tlb_flush_all();
}

/*
 * Page table traversal callbacks
 */

void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
    uint64_t pte = *ptep;
    if (!(PT_ET(pte) == PT_ET_PTE) || !(pte & PTE_M))
	return;

    struct page_info *pi = page_to_pageinfo(pa2kva(PTE_ADDR(pte)));
    pi->pi_dirty = 1;
    *ptep &= ~PTE_M;
}

void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
    as_arch_collect_dirty_bits(arg, ptep, va);

    uint32_t pte = *ptep;
    if (PT_ET(pte) == PT_ET_PTE) {
	if (PTE_ACC(pte) & PTE_ACC_W)
	    pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep = 0;
    }
}

void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
    as_arch_collect_dirty_bits(arg, ptep, va);
    
    uint32_t pte = *ptep;
    if ((PT_ET(pte) == PT_ET_PTE) && (PTE_ACC(pte) & PTE_ACC_W)) {
	pagetree_decpin(pa2kva(PTE_ADDR(pte)));
	*ptep &= ~(PTE_ACC_W << PTE_ACC_SHIFT);
    }
}

int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
    /* The LEON MMU doesn't seem to work as expected unless readable and
     * writable pages are marked executable.  Linux does the same thing
     * for its SRMMU implementation: include/asm-sparc/pgtsrmmu.h and
     * arch/sparc/mm/srmmu.c
     */
    uint32_t ptflags = PTE_ACC_X;
    if (flags & SEGMAP_WRITE)
	ptflags |= PTE_ACC_W;
    if (flags & SEGMAP_EXEC)
	ptflags |= PTE_ACC_X;

    ptent_t *ptep;
    int r = pgdir_walk(pgmap, va, 1, &ptep);
    if (r < 0) {
	cprintf("XXX pgdir_walk error: %s\n", e2s(r));
	return r;
    }

    as_arch_page_invalidate_cb(pgmap, ptep, va);
    *ptep = PTE_ENTRY(kva2pa(pp), ptflags);
    if (ptflags & PTE_ACC_W)
    	pagetree_incpin(pp);

    return 0;
}

/*
 * Page addressing
 */

void *
pa2kva(physaddr_t pa)
{
    return (void *) (pa + LOAD_OFFSET);
}

physaddr_t
kva2pa(void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= PHYSBASE)
	return va - LOAD_OFFSET;
    panic("kva2pa called with invalid kva %p", kva);
}

ppn_t
pa2ppn(physaddr_t pa)
{
    assert(mem_detect_done);
    return ((pa - minpa) >> PGSHIFT);
}

physaddr_t
ppn2pa(ppn_t pn)
{
    assert(mem_detect_done);
    return (pn << PGSHIFT) + minpa;
}
