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
pmap_set_current_arch(struct Pagemap *pm)
{
    if (!pm)
	pm = &bootpd;

    lcr3(kva2pa(pm));
}
