#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <inc/container.h>
#include <machine/types.h>
#include <kern/label.h>

struct Gate {
    void *gt_entry;
    void *gt_stack;
    uint64_t gt_arg;

    struct Label *gt_recv_label;
    struct Label *gt_send_label;

    // target address space
    struct cobj_ref gt_pmap_cobj;

    uint32_t gt_ref;
};

int  gate_alloc(struct Gate **gp);
void gate_free(struct Gate *g);
void gate_decref(struct Gate *g);

#endif
