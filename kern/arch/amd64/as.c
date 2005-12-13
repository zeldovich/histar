#include <machine/x86.h>
#include <machine/as.h>
#include <kern/segment.h>
#include <inc/error.h>

int
as_alloc(struct Label *l, struct Address_space **asp)
{
    struct Address_space *as;
    int r = kobject_alloc(kobj_address_space, l, (struct kobject **) &as);
    if (r < 0)
	return r;

    memset(&as->as_segmap, 0, sizeof(as->as_segmap));
    as_swapin(as);

    *asp = as;
    return 0;
}

static void
as_invalidate(struct Address_space *as)
{
    as_swapout(as);
    as_swapin(as);
}

int
as_to_user(struct Address_space *as, struct u_address_space *uas)
{
    int r = page_user_incore((void**) &uas, sizeof(*uas));
    if (r < 0)
	return r;

    uint64_t size = uas->size;
    struct segment_mapping *ents = uas->ents;
    r = page_user_incore((void**) &ents, sizeof(*ents) * size);
    if (r < 0)
	return r;

    uint64_t nent = 0;
    for (uint64_t i = 0; i < NSEGMAP; i++) {
	if (as->as_segmap[i].flags == 0)
	    continue;

	if (nent >= size)
	    return -E_NO_SPACE;

	ents[nent] = as->as_segmap[i];
	nent++;
    }

    uas->nent = nent;
    return 0;
}

int
as_from_user(struct Address_space *as, struct u_address_space *uas)
{
    int r = page_user_incore((void**) &uas, sizeof(*uas));
    if (r < 0)
	return r;

    uint64_t nent = uas->nent;
    struct segment_mapping *ents = uas->ents;
    r = page_user_incore((void**) &ents, sizeof(*ents) * nent);
    if (r < 0)
	return r;

    if (nent > NSEGMAP)
	return -E_NO_SPACE;

    memset(&as->as_segmap, 0, sizeof(as->as_segmap));
    for (uint64_t i = 0; i < nent; i++)
	as->as_segmap[i] = ents[i];

    as_invalidate(as);
    return 0;
}

void
as_swapin(struct Address_space *as)
{
    as->as_pgmap = &bootpml4;
}

void
as_swapout(struct Address_space *as)
{
    if (as->as_pgmap && as->as_pgmap != &bootpml4)
	page_map_free(as->as_pgmap);
}

int
as_gc(struct Address_space *as)
{
    as_swapout(as);
    return 0;
}

int
as_pagefault(struct Address_space *as, void *va)
{
    if (as->as_pgmap == &bootpml4) {
	int r = page_map_alloc(&as->as_pgmap);
	if (r < 0)
	    return r;
    }

    int r = segment_map_fill_pmap(&as->as_segmap[0], as->as_pgmap, va);
    if (r == -E_RESTART)
	return r;

    if (r < 0) {
	cprintf("as_pagefault(%ld, %p): %s\n", as->as_ko.ko_id, va, e2s(r));
	return r;
    }

    return 0;
}

void
as_switch(struct Address_space *as)
{
    struct Pagemap *pgmap = as ? as->as_pgmap : &bootpml4;
    lcr3(kva2pa(pgmap));
}
