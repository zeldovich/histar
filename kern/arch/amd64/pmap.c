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
