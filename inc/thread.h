#ifndef JOS_INC_THREAD_H
#define JOS_INC_THREAD_H

#include <inc/types.h>
#include <inc/segment.h>

struct thread_entry {
    struct cobj_ref te_as;
    void *te_entry;
    void *te_stack;
    uint64_t te_arg;
};

#endif
