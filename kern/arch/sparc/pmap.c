#include <kern/arch.h>
#include <kern/lib.h>
#include <machine/sparc-common.h>
#include <inc/error.h>

#define perr() cprintf("%s:%u: XXX unimpl\n", __FILE__, __LINE__)

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

void
page_map_free(struct Pagemap *pgmap)
{
    /* XXX free all page table levels */
    perr();
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
    perr();
    return -E_INVAL;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
    perr();
    return -E_INVAL;
}

int
check_user_access(const void *ptr, uint64_t nbytes, uint32_t reqflags)
{
    perr();
    return -E_INVAL;
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
    
    /* flush entire TLB (pg 249-250 SPARC v8 manual) */
    sta_mmuflush(0x400);
    /* flush both icache and dcache */
    flush();
    sta_dflush();
}

/*
 * Page table traversal callbacks
 */

void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
    perr();
    //const struct Pagemap *pgmap = arg;
}

void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    perr();
    //const struct Pagemap *pgmap = arg;
}

void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
    perr();
    //const struct Pagemap *pgmap = arg;
}

int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
    uint64_t ptflags = 0;
    if ((flags & SEGMAP_WRITE))
	ptflags |= PTE_ACC_W;
    if ((flags & SEGMAP_EXEC))
	ptflags |= PTE_ACC_X;
    

    perr();
    return -E_INVAL;
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
    return (pa >> PGSHIFT);
}

physaddr_t
ppn2pa(ppn_t pn)
{
    return (pn << PGSHIFT);
}
