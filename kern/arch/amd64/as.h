#ifndef JOS_KERN_AS_H
#define JOS_KERN_AS_H

#include <machine/pmap.h>
#include <machine/types.h>
#include <kern/kobj.h>
#include <inc/segment.h>
#include <inc/queue.h>

#define NSEGMAP 32

struct Address_space;

struct segment_mapping {
    struct u_segment_mapping sm_usm;

    struct Address_space *sm_as;
    struct Segment *sm_sg;
    LIST_ENTRY(segment_mapping) sm_link;
};

LIST_HEAD(segmap_list, segment_mapping);

struct Address_space {
    struct kobject as_ko;

    struct Pagemap *as_pgmap;
    struct segment_mapping as_segmap[NSEGMAP];
};

int  as_alloc(struct Label *l, struct Address_space **asp);
int  as_to_user(struct Address_space *as, struct u_address_space *uas);
int  as_from_user(struct Address_space *as, struct u_address_space *uas);

void as_swapin(struct Address_space *as);
void as_swapout(struct Address_space *as);
int  as_gc(struct Address_space *as);

int  as_pagefault(struct Address_space *as, void *va);
void as_switch(struct Address_space *as);

#endif
