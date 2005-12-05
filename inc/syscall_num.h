#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

#include <inc/container.h>

typedef enum {
    SYS_yield = 0,
    SYS_halt,

    SYS_cputs,
    SYS_cgetc,

    SYS_container_alloc,
    SYS_container_nslots,
    SYS_container_store_cur_thread,

    SYS_obj_unref,
    SYS_obj_get_type,
    SYS_obj_get_id,	// get global id of an object (useful for containers)
    SYS_obj_get_label,

    SYS_handle_create,

    SYS_gate_create,
    SYS_gate_enter,

    SYS_thread_create,
    SYS_thread_start,

    SYS_segment_create,
    SYS_segment_resize,
    SYS_segment_get_npages,
    SYS_segment_get_map,

    NSYSCALLS
} syscall_num;

#endif
