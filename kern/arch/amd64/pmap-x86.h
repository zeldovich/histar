#ifndef JOS_MACHINE_PMAP_X86_H
#define JOS_MACHINE_PMAP_X86_H

static __inline ppn_t
pa2ppn(physaddr_t pa)
{
    ppn_t pn = pa >> PGSHIFT;
    if (pn > global_npages)
	panic("pa2ppn: pa 0x%lx out of range, npages %"PRIu64,
	      (unsigned long) pa, global_npages);
    return pn;
}

static __inline physaddr_t
ppn2pa(ppn_t pn)
{
    if (pn > global_npages)
	panic("ppn2pa: ppn %lx out of range, npages %"PRIu64,
	      (unsigned long) pn, global_npages);
    return (pn << PGSHIFT);
}

#endif
