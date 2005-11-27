#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

typedef enum {
    SYS_cputs = 0,
    SYS_yield,
    SYS_halt,

    SYS_container_alloc,
    SYS_container_unref,
    SYS_container_get_type,	// get type of contained object
    SYS_container_get_c_idx,	// get global index of sub-container

    SYS_container_store_cur_thread,
    SYS_container_store_cur_addrspace,

    NSYSCALLS
} syscall_num;

#endif
