#ifndef JOS_INC_SYSCALL_PARAM_H
#define JOS_INC_SYSCALL_PARAM_H

#include <inc/types.h>
#include <inc/container.h>
#include <inc/segment.h>

struct sys_segment_map_args {
    struct cobj_ref segment;
    struct cobj_ref pmap;
    void *va;
    uint64_t start_page;
    uint64_t num_pages;
    segment_map_mode mode;
};

struct sys_gate_create_args {
    uint64_t container;
    void *entry;
    void *stack;
    uint64_t arg;
    struct cobj_ref pmap;
};

#endif
