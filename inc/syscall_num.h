#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

typedef enum {
    SYS_cons_puts = 0,
    SYS_cons_getc,

    SYS_net_create,
    SYS_net_wait,
    SYS_net_buf,
    SYS_net_macaddr,

    SYS_container_alloc,
    SYS_container_nslots,
    SYS_container_get_slot_id,

    SYS_obj_unref,
    SYS_obj_get_type,
    SYS_obj_get_label,
    SYS_obj_get_name,

    SYS_handle_create,

    SYS_gate_create,
    SYS_gate_enter,
    SYS_gate_send_label,

    SYS_thread_create,
    SYS_thread_start,
    SYS_thread_yield,
    SYS_thread_halt,
    SYS_thread_id,
    SYS_thread_addref,
    SYS_thread_set_label,

    SYS_thread_get_as,
    SYS_thread_set_as,

    SYS_thread_sync_wait,
    SYS_thread_sync_wakeup,

    SYS_clock_msec,

    SYS_segment_create,
    SYS_segment_copy,
    SYS_segment_resize,
    SYS_segment_get_npages,

    SYS_as_create,
    SYS_as_get,
    SYS_as_set,

    SYS_mlt_create,
    SYS_mlt_put,
    SYS_mlt_get,

    NSYSCALLS
} syscall_num;

#endif
