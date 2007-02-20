#include <machine/x86.h>
#include <machine/pmap.h>
#include <kern/lib.h>
#include <kern/thread.h>
#include <inc/error.h>
#include <inc/safeint.h>
#include <inc/intmacro.h>

int
page_map_alloc(struct Pagemap **pm_store)
{
    void *pmap;
    int r = page_alloc(&pmap);
    if (r < 0)
	return r;

    memcpy(pmap, &bootpml4, PGSIZE);
    *pm_store = (struct Pagemap *) pmap;
    return 0;
}

static void
page_map_free_level(struct Pagemap *pgmap, int pmlevel)
{
    // Skip the kernel half of the address space
    int maxi = (pmlevel == 3 ? NPTENTRIES/2 : NPTENTRIES);
    int i;

    for (i = 0; i < maxi; i++) {
	uint64_t ptent = pgmap->pm_ent[i];
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
    page_map_free_level(pgmap, 3);
}

static int
page_map_traverse_internal(struct Pagemap *pgmap, int pmlevel,
			   const void *first, const void *last,
			   int create,
			   page_map_traverse_cb cb, const void *arg,
			   void *va_base)
{
    int r;
    assert(pmlevel >= 0 && pmlevel <= 3);

    uint32_t first_idx = PDX(pmlevel, first);
    uint32_t last_idx  = PDX(pmlevel, last);

    for (uint64_t idx = first_idx; idx <= last_idx; idx++) {
	uint64_t *pm_entp = &pgmap->pm_ent[idx];
	uint64_t pm_ent = *pm_entp;

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
	const void *last_next  = (idx == last_idx)  ? last : (const void *) UINT64(~0);
	r = page_map_traverse_internal(pm_next, pmlevel - 1, first_next, last_next,
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
    return page_map_traverse_internal(pgmap, 3, first, last, create, cb, arg, 0);
}

static void
pgdir_walk_cb(const void *arg, uint64_t *ptep, void *va)
{
    uint64_t **pte_store = (uint64_t **) arg;
    *pte_store = ptep;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, uint64_t **pte_store)
{
    *pte_store = 0;
    int r = page_map_traverse(pgmap, va, va, create, &pgdir_walk_cb, pte_store);
    if (create && !*pte_store)
	return -E_INVAL;
    return r;
}

//
// Return the page mapped at virtual address 'va'.
// If pte_store is not zero, then we store in it the address
// of the pte for this page.  This is used by page_remove
// but should not be used by other callers.
//
// Return 0 if there is no page mapped at va.
//
static void *
page_lookup(struct Pagemap *pgmap, void *va, uint64_t **pte_store)
{
    if ((uintptr_t) va >= ULIM)
	panic("page_lookup: va %p over ULIM", va);

    uint64_t *ptep;
    int r = pgdir_walk(pgmap, va, 0, &ptep);
    if (r < 0)
	panic("pgdir_walk(%p, create=0) failed: %d", va, r);

    if (pte_store)
	*pte_store = ptep;

    if (ptep == 0 || !(*ptep & PTE_P))
	return 0;

    return pa2kva (PTE_ADDR (*ptep));
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

    uint64_t pte_flags = PTE_P | PTE_U;
    if ((reqflags & SEGMAP_WRITE))
	pte_flags |= PTE_W;

    int aspf = 0;
    if (nbytes > 0) {
	int overflow = 0;
	uintptr_t iptr = (uintptr_t) ptr;
	uintptr_t start = ROUNDDOWN(iptr, PGSIZE);
	uintptr_t end = ROUNDUP(safe_add(&overflow, iptr, nbytes), PGSIZE);

	if (end <= start || overflow)
	    return -E_INVAL;

	for (uintptr_t va = start; va < end; va += PGSIZE) {
	    if (va >= ULIM)
		return -E_INVAL;

	    uint64_t *ptep;
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

void
pmap_tlb_invlpg(const void *va)
{
    invlpg(va);
}

void
pmap_set_current(struct Pagemap *pm, int flush_tlb)
{
    if (!pm)
	pm = &bootpml4;

    uint64_t new_cr3 = kva2pa(pm);

    if (!flush_tlb) {
	uint64_t cur_cr3 = rcr3();
	if (cur_cr3 == new_cr3)
	    return;
    }

    lcr3(new_cr3);
}
