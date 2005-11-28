#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

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

    NSYSCALLS
} syscall_num;

#endif
