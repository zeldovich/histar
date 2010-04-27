#ifndef JOS_KERN_RESERVE_H
#define JOS_KERN_RESERVE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>

struct Reserve {
    struct kobject_hdr rs_ko;

    int64_t rs_level;
    int64_t rs_consumed;
    int64_t rs_decayed;

    uint64_t rs_linked;
    LIST_ENTRY(Reserve) rs_link;
};
LIST_HEAD(Reserve_list, Reserve);

int reserve_alloc(const struct Label *l, struct Reserve **rsp);
int reserve_transfer(struct cobj_ref sourceref, struct cobj_ref sinkref, int64_t amount);
int64_t reserve_consume(struct Reserve *rs, int64_t amount, uint64_t force);
int reserve_gc(struct Reserve *rs);
void reserve_decay_all(uint64_t now);
int reserve_transfer_proportional(struct cobj_ref sourceref, struct cobj_ref sinkref, int64_t frac);
void reserve_prof_toggle(void);

extern struct Reserve *root_rs;

#endif
