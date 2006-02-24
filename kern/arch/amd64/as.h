#ifndef JOS_KERN_AS_H
#define JOS_KERN_AS_H

#include <machine/pmap.h>
#include <machine/types.h>
#include <kern/kobjhdr.h>
#include <kern/pagetree.h>
#include <inc/segment.h>
#include <inc/queue.h>

struct Address_space;

struct segment_mapping {
    const struct Address_space *sm_as;
    uint64_t sm_as_slot;

    const struct Segment *sm_sg;
    LIST_ENTRY(segment_mapping) sm_link;
};

LIST_HEAD(segmap_list, segment_mapping);

#define N_USEGMAP_PER_PAGE	(PGSIZE / sizeof(struct u_segment_mapping))
#define N_SEGMAP_PER_PAGE	(PGSIZE / sizeof(struct segment_mapping))

struct Address_space {
    struct kobject_hdr as_ko;

    struct pagetree as_segmap_pt;
    struct Pagemap *as_pgmap;
    kobject_id_t as_pgmap_tid;
};

int  as_alloc(const struct Label *l, struct Address_space **asp);
int  as_to_user(const struct Address_space *as, struct u_address_space *uas);
int  as_from_user(struct Address_space *as, struct u_address_space *uas);

void as_swapin(struct Address_space *as);
void as_swapout(struct Address_space *as);
int  as_gc(struct Address_space *as);
void as_invalidate(const struct Address_space *as);

int  as_pagefault(struct Address_space *as, void *va);
void as_switch(const struct Address_space *as);

#endif
