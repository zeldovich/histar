#ifndef JOS_INC_THREAD_H
#define JOS_INC_THREAD_H

#include <inc/types.h>
#include <inc/container.h>

struct thread_entry {
    struct cobj_ref te_pmap;
    int te_pmap_copy;

    void *te_entry;
    void *te_stack;
    uint64_t te_arg;
};

#endif
