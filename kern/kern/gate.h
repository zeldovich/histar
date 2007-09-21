#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>
#include <inc/thread.h>

struct Gate {
    struct kobject_hdr gt_ko;

    struct thread_entry gt_te;
    uint8_t gt_te_visible;
    uint8_t gt_te_unspec;
};

int  gate_alloc(const struct Label *l,
		const struct Label *clearance,
		const struct Label *verify,
		struct Gate **gp)
    __attribute__ ((warn_unused_result));

#endif
