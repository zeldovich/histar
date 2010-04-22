#ifndef JOS_KERN_RESERVE_H
#define JOS_KERN_RESERVE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>

struct Reserve {
    struct kobject_hdr rs_ko;

    int64_t rs_level;

    uint64_t rs_linked;
    LIST_ENTRY(Reserve) rs_link;
};
LIST_HEAD(Reserve_list, Reserve);

// ONLY CALL THIS IN INIT - all others need to be splits
int reserve_alloc(const struct Label *l, struct Reserve **rsp);
int reserve_split(const struct Label *l, struct Reserve *origrs, struct Reserve **newrsp, int64_t new_level);
int reserve_transfer(struct cobj_ref sourceref, struct cobj_ref sinkref, int64_t amount);
int64_t reserve_consume(struct Reserve *rs, int64_t amount, uint64_t force);

#endif