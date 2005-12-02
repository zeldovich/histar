#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/kobj.h>
#include <inc/thread.h>

struct Gate {
    struct kobject gt_ko;

    struct Label *gt_target_label;
    struct thread_entry gt_te;
};

int  gate_alloc(struct Label *l, struct Gate **gp);
void gate_gc(struct Gate *g);

#endif
