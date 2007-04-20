#include <kern/arch/amd64/pmap-x86.c>

int
page_map_alloc(struct Pagemap **pm_store)
{
    void *pmap;
    int r = page_alloc(&pmap);
    if (r < 0)
	return r;

    memcpy(pmap, &bootpd, PGSIZE);
    *pm_store = (struct Pagemap *) pmap;
    return 0;
}

void
pmap_set_current(struct Pagemap *pm, int flush_tlb)
{
    if (!pm)
	pm = &bootpd;

    uint32_t new_cr3 = kva2pa(pm);

    if (!flush_tlb) {
	uint32_t cur_cr3 = rcr3();
	if (cur_cr3 == new_cr3)
	    return;
    }

    lcr3(new_cr3);
}

void *
pa2kva(physaddr_t pa)
{
    if (pa < 0x40000000U)
	return (void *) (pa + PHYSBOT);
    if (pa >= 0xc0000000U)
	return (void *) (pa);
    panic("cannot map physaddr %x\n", pa);
}

physaddr_t
kva2pa(void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= PHYSBOT && va < PHYSBOT + (global_npages << PGSHIFT))
	return va - PHYSBOT;
    if (va >= PHYSTOP)
	return va;
    panic("kva2pa called with invalid kva %p", kva);
}
