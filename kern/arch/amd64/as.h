#ifndef JOS_KERN_AS_H
#define JOS_KERN_AS_H

#include <machine/pmap.h>
#include <machine/types.h>
#include <kern/kobj.h>
#include <inc/segment.h>

#define NSEGMAP 32
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
