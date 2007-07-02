#include <kern/arch.h>
#include <kern/lib.h>
#include <inc/error.h>

int
page_map_alloc(struct Pagemap **pm_store)
{
    return -E_NO_MEM;
}

void
page_map_free(struct Pagemap *pgmap)
{
    /* XXX free all page table levels */
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
    return -E_INVAL;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
    return -E_INVAL;
}

int
check_user_access(const void *ptr, uint64_t nbytes, uint32_t reqflags)
{
    return -E_INVAL;
}

void
pmap_set_current(struct Pagemap *pm)
{
    cprintf("pmap_set_current: XXX\n");
}

/*
 * Page table traversal callbacks
 */

void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
}

void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
}

void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
    //const struct Pagemap *pgmap = arg;
}

int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
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
