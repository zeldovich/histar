#ifndef JOS_INC_THREAD_H
#define JOS_INC_THREAD_H

#include <inc/types.h>
#include <inc/segment.h>

struct thread_entry {
    struct segment_map te_segmap;

    void *te_entry;
    void *te_stack;
    uint64_t te_arg;
};

#endif
