#include <machine/pmap.h>
#include <machine/lnxpage.h>
#include <machine/lnxopts.h>
#include <machine/ftglue.h>
#include <kern/arch.h>
#include <kern/sched.h>
#include <inc/error.h>
#include <inc/safeint.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <assert.h>

static struct Pagemap *cur_pm;
enum { lnxpmap_debug = 0 };

static void *the_user_page;
void
lnxpmap_set_user_concr_page(void *base)
{
    the_user_page = base;
}

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

    if (nbytes > 0) {
	void *orig_base = base;

	int overflow = 0;
	uintptr_t ibase = (uintptr_t) base;
	uintptr_t end = safe_addptr(&overflow, base, nbytes);
	base = ROUNDDOWN(base, PGSIZE);
	end = ROUNDUP(end, PGSIZE);
	if (end <= ibase || overflow)
	    return -E_INVAL;

	for (void *va = (void *) base; va < (void *) end; va += PGSIZE) {
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

	if (the_user_page) {
	    /*
	     * Concretize the pointer for FT
	     */
	    assert(orig_base >= the_user_page);
	    assert(orig_base <= the_user_page + PGSIZE - nbytes);

	    static uint64_t next_offset;
	    if (next_offset + nbytes > PGSIZE)
		panic("check_user_access: overflowed symbolic user page; offset %"PRIu64, next_offset);

	    ft_assume(orig_base == (the_user_page + next_offset));
	    cprintf("check_user_access: concretizing %"PRIu64" bytes at offset %"PRIu64"\n",
		    nbytes, next_offset);
	    next_offset += nbytes;
	}
    }

    as_switch(cur_as);
    return 0;
}

int
page_map_alloc(struct Pagemap **pm_store)
{
    if (enable_page_alloc_failure && FT_CHOOSE(2))
	return -E_NO_MEM;

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
    const void *base = ROUNDDOWN(va, PGSIZE);
    int freeslot = -1;

    for (int i = 0; i < NPME; i++) {
	if (pgmap->pme[i].pte & PTE_P) {
	    if (pgmap->pme[i].va == base) {
		*pte_store = &pgmap->pme[i].pte;
		return 0;
	    }
	} else {
	    if (freeslot == -1)
		freeslot = i;
	}
    }

    if (!create) {
	*pte_store = 0;
	return 0;
    }

    if (enable_page_alloc_failure && FT_CHOOSE(2))
	return -E_NO_MEM;

    if (freeslot == -1)
	return -E_NO_MEM;

    pgmap->pme[freeslot].va = (void *) base;
    pgmap->pme[freeslot].pte = 0;
    *pte_store = &pgmap->pme[freeslot].pte;
    return 0;
}

void
pmap_set_current(struct Pagemap *pm, int flush_tlb)
{
    cur_pm = pm;
}
