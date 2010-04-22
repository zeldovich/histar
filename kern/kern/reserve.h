#ifndef JOS_KERN_RESERVE_H
#define JOS_KERN_RESERVE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>

struct Reserve {
    struct kobject_hdr rs_ko;

    uint64_t rs_level;
};

// ONLY CALL THIS IN INIT - all others need to be splits
int reserve_alloc(const struct Label *l, struct Reserve **rsp);
int reserve_split(const struct Label *l, struct Reserve *origrs, struct Reserve **newrsp, uint64_t new_level);
int reserve_tranfser(struct cobj_ref sourceref, struct cobj_ref sinkref, uint64_t amount);

#endif
