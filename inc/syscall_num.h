#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

#include <inc/container.h>

typedef enum {
    SYS_yield = 0,
    SYS_halt,

    SYS_cputs,
    SYS_cgetc,

    SYS_container_alloc,
    SYS_container_unref,
    SYS_container_get_type,	// get type of contained object
    SYS_container_get_c_idx,	// get global index of sub-container

    SYS_container_store_cur_thread,
    SYS_container_store_cur_addrspace,

    SYS_gate_create,
    SYS_gate_enter,

    SYS_thread_create,

    SYS_addrspace_create,	// XXX not implemented yet

    SYS_segment_create,
    SYS_segment_resize,
    SYS_segment_get_npages,
    SYS_segment_map,

    NSYSCALLS
} syscall_num;

typedef enum {
    segment_map_ro = 0,
    segment_map_rw,
    segment_map_cow
} segment_map_mode;

struct segment_map_args {
    struct cobj_ref segment;
    struct cobj_ref as;
    void *va;
    uint64_t start_page;
    uint64_t num_pages;
    segment_map_mode mode;
};

#endif
