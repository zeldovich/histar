#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <inc/error.h>

int
page_map_alloc(struct Pagemap **pm_store)
{
    return -E_NO_MEM;
}

void
page_map_free(struct Pagemap *pgmap)
{
    cprintf("page_map_free");
}

int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
    return -E_NO_MEM;
}

int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
    return -E_NO_MEM;
}

int
check_user_access2(const void *ptr, uint64_t nbytes,
		   uint32_t reqflags, int alignbytes)
{
    return -E_NO_MEM;
}

void
pmap_set_current(struct Pagemap *pm)
{
    cprintf("pmap_set_current");
}

/*
 * Page table traversal callbacks
 */

void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
    cprintf("%s\n", __func__);
}

void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
    cprintf("%s\n", __func__);
}

void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
    cprintf("%s\n", __func__);
}

int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
    cprintf("%s\n", __func__);
    return -E_NO_MEM;
}

/*
 * Page addressing
 */

void *
pa2kva(physaddr_t pa)
{
    cprintf("%s\n", __func__);
    return 0;

    panic("pa2kva called with invalid pa 0x%x", pa);
}

physaddr_t
kva2pa(void *kva)
{
    cprintf("%s\n", __func__);
    return 0;

    panic("kva2pa called with invalid kva %p", kva);
}

ppn_t
pa2ppn(physaddr_t pa)
{
    ppn_t npage = pa >> PGSHIFT;
    if (npage > global_npages)
	panic("pa2ppn: 0x%x too far out", pa);
    return npage;
}

physaddr_t
ppn2pa(ppn_t pn)
{
    return pn << PGSHIFT;
}
