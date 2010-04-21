#ifndef JOS_KERN_LIMIT_H
#define JOS_KERN_LIMIT_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>

struct Limit {
    struct kobject_hdr lm_ko;

    kobject_id_t source;
    kobject_id_t sink;

    uint64_t rate;
    uint64_t type;  // 0 is constant, 1 is proportional

    uint64_t level;
    uint64_t limit;
};

#endif
