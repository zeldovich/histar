#ifndef JOS_INC_GATE_H
#define JOS_INC_GATE_H

#include <inc/container.h>
#include <inc/atomic.h>

// The layout of container and entry_stack_use fields of this structure
// is hard-coded in assembly.  Don't move them around.
struct u_gate_entry {
    uint64_t container;
    atomic_t entry_stack_use;

    struct cobj_ref gate;
    void *stackbase;

    void (*func) (void *, struct cobj_ref *);
    void *func_arg;
};

// gate.c
int	gate_create(struct u_gate_entry *ug, uint64_t container,
		    void (*func)(void*, struct cobj_ref*), void *func_arg,
		    char *name);
int	gate_call(uint64_t ctemp, struct cobj_ref gate, struct cobj_ref *arg);

#endif
