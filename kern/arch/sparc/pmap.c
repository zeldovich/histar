#include <kern/arch.h>
#include <kern/lib.h>

void *
pa2kva(physaddr_t pa)
{
    return (void *) (pa + PHYSBASE);
}

physaddr_t
kva2pa(void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= PHYSBASE)
	return va - PHYSBASE;
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
