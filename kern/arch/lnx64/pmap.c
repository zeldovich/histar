#include <machine/pmap.h>
#include <machine/lnxpage.h>
#include <machine/lnxopts.h>
#include <kern/arch.h>
#include <kern/sched.h>
#include <inc/error.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <assert.h>

#ifdef FT_TRANSFORMED
#include <ft_runtest.h>
#endif

static struct Pagemap *cur_pm;
enum { lnxpmap_debug = 0 };

void
lnxpmap_init(void)
{
    /* Nothing, we have a really simple page mapping layer now. */
}

int
check_user_access(const void *base, uint64_t nbytes, uint32_t reqflags)
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
    if (reqflags & SEGMAP_WRITE)
	pte_flags |= PTE_W;

    const void *end = base + nbytes;
    if (end < base)
	return -E_INVAL;

    if (nbytes > 0) {
	for (void *va = (void *) ROUNDDOWN(base, PGSIZE);
	     va < ROUNDUP(end, PGSIZE); va += PGSIZE)
	{
	    int va_ok = 0;
	    for (int i = 0; cur_as->as_pgmap && i < NPME; i++) {
		struct Pagemapent *pme = &cur_as->as_pgmap->pme[i];
		if (pme->va == va && pa2kva(PTE_ADDR(pme->pte)) == va && (pme->pte & pte_flags) == pte_flags) {
		    va_ok = 1;
		    break;
		}
	    }

	    if (!va_ok) {
		int r = as_pagefault(cur_as, va, reqflags);
		if (r < 0)
		    return r;
	    }
	}
    }

    as_switch(cur_as);
    return 0;
}

int
page_map_alloc(struct Pagemap **pm_store)
{
#ifdef FT_TRANSFORMED
    if (enable_page_alloc_failure && FT_CHOOSE(2))
	return -E_NO_MEM;
#endif

    struct Pagemap *pm = malloc(sizeof(*pm));
    memset(pm, 0, sizeof(*pm));
    *pm_store = pm;
    return 0;
}

void
page_map_free(struct Pagemap *pgmap)
{
    free(pgmap);
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
    assert(!create);
    for (int i = 0; i < NPME; i++)
	if ((pgmap->pme[i].pte & PTE_P) && pgmap->pme[i].va >= first && pgmap->pme[i].va <= last)
	    cb(arg, &pgmap->pme[i].pte, pgmap->pme[i].va);
    return 0;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, uint64_t **pte_store)
{
    assert(create);
    assert(!PGOFF(va));

#ifdef FT_TRANSFORMED
    if (enable_page_alloc_failure && FT_CHOOSE(2))
	return -E_NO_MEM;
#endif

    int freeslot = -1;

    for (int i = 0; i < NPME; i++) {
	if (pgmap->pme[i].pte & PTE_P) {
	    if (pgmap->pme[i].va == va) {
		*pte_store = &pgmap->pme[i].pte;
		return 0;
	    }
	} else {
	    if (freeslot == -1)
		freeslot = i;
	}
    }

    if (freeslot == -1)
	return -E_NO_MEM;

    pgmap->pme[freeslot].va = (void *) va;
    pgmap->pme[freeslot].pte = 0;
    *pte_store = &pgmap->pme[freeslot].pte;
    return 0;
}

void
pmap_tlb_invlpg(const void *va)
{
}

void
pmap_set_current(struct Pagemap *pm, int flush_tlb)
{
    cur_pm = pm;
}
