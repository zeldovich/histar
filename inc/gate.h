#ifndef JOS_INC_GATE_H
#define JOS_INC_GATE_H

#include <inc/container.h>
#include <inc/label.h>

struct u_gate_entry {
    uint64_t container;
    struct cobj_ref gate;
    void *stackbase;

    void (*func) (void *, struct cobj_ref *);
    void *func_arg;
};

// gate.c
int	gate_create(struct u_gate_entry *ug,
		    uint64_t gate_container,
		    uint64_t entry_container,
		    void (*func)(void*, struct cobj_ref*), void *func_arg,
		    const char *name, struct ulabel *l_send);
int	gate_call(struct cobj_ref gate, struct cobj_ref *arg);

#endif
