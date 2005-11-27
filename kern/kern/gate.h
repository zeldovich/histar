#ifndef JOS_KERN_GATE_H
#define JOS_KERN_GATE_H

#include <machine/types.h>

struct Gate {
    void *gt_entry;
    uint64_t gt_arg;

    // target address space
    uint64_t gt_as_container;
    uint32_t gt_as_idx;

    uint32_t gt_ref;
};

int  gate_alloc(struct Gate **gp);
void gate_free(struct Gate *g);
void gate_decref(struct Gate *g);

#endif
