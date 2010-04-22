#ifndef JOS_KERN_LIMIT_H
#define JOS_KERN_LIMIT_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>

struct Limit {
    struct kobject_hdr lm_ko;

    struct cobj_ref lm_source;
    struct cobj_ref lm_sink;

    uint64_t lm_rate;
    uint64_t lm_type;  // 0 is constant, 1 is proportional

    uint64_t lm_level;
    uint64_t lm_limit;

    uint64_t lm_linked;
    LIST_ENTRY(Limit) lm_link;
};
LIST_HEAD(Limit_list, Limit);

int limit_gc(struct Limit *lm);
int limit_create(const struct Label *l, struct cobj_ref sourcersref,
	     struct cobj_ref sinkrsref, struct Limit **lmp);
int limit_set_rate(struct Limit *lm, uint64_t rate);
void limit_update_all();


#endif
