#include <machine/x86.h>
#include <machine/pmap.h>
#include <kern/lib.h>
#include <kern/thread.h>
#include <kern/arch.h>
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

void *
pa2kva (physaddr_t pa)
{
    return (void*) (pa + PHYSBASE);
}

physaddr_t
kva2pa (void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= KERNBASE && va < KERNBASE + (global_npages << PGSHIFT))
	return va - KERNBASE;
    if (va >= PHYSBASE && va < PHYSBASE + (global_npages << PGSHIFT))
	return va - PHYSBASE;
    panic("kva2pa called with invalid kva %p", kva);
}

ppn_t
pa2ppn (physaddr_t pa)
{
    ppn_t pn = pa >> PGSHIFT;
    if (pn > global_npages)
	panic("pa2ppn: pa 0x%lx out of range, npages %ld", pa, global_npages);
    return pn;
}

physaddr_t
ppn2pa (ppn_t pn)
{
    if (pn > global_npages)
	panic("ppn2pa: ppn %ld out of range, npages %ld", pn, global_npages);
    return (pn << PGSHIFT);
}
