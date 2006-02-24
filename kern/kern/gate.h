#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>
#include <inc/thread.h>

struct Gate {
    struct kobject_hdr gt_ko;

    struct Label gt_clearance;
    struct thread_entry gt_te;
};

int  gate_alloc(const struct Label *l,
		const struct Label *clearance,
		struct Gate **gp);

#endif
