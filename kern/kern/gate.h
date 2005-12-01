#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <machine/types.h>
#include <kern/label.h>
#include <inc/thread.h>

struct Gate {
    struct Label *gt_recv_label;
    struct Label *gt_send_label;

    uint32_t gt_ref;

    struct thread_entry gt_te;
};

int  gate_alloc(struct Gate **gp);
void gate_free(struct Gate *g);
void gate_decref(struct Gate *g);

#endif
