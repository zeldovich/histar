#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

#define ALL_SYSCALLS \
    SYSCALL_ENTRY(cons_puts)			\
    SYSCALL_ENTRY(cons_getc)			\
    SYSCALL_ENTRY(cons_cursor)			\
    SYSCALL_ENTRY(cons_probe)			\
    SYSCALL_ENTRY(net_create)			\
    SYSCALL_ENTRY(net_wait)			\
    SYSCALL_ENTRY(net_buf)			\
    SYSCALL_ENTRY(net_macaddr)			\
    SYSCALL_ENTRY(container_alloc)		\
    SYSCALL_ENTRY(container_get_nslots)		\
    SYSCALL_ENTRY(container_get_parent)		\
    SYSCALL_ENTRY(container_get_slot_id)	\
    SYSCALL_ENTRY(container_get_avail_quota)	\
    SYSCALL_ENTRY(container_move_quota)		\
    SYSCALL_ENTRY(obj_unref)			\
    SYSCALL_ENTRY(obj_get_type)			\
    SYSCALL_ENTRY(obj_get_label)		\
    SYSCALL_ENTRY(obj_get_name)			\
    SYSCALL_ENTRY(obj_get_reserve)		\
    SYSCALL_ENTRY(handle_create)		\
    SYSCALL_ENTRY(gate_create)			\
    SYSCALL_ENTRY(gate_enter)			\
    SYSCALL_ENTRY(gate_clearance)		\
    SYSCALL_ENTRY(thread_create)		\
    SYSCALL_ENTRY(thread_start)			\
    SYSCALL_ENTRY(thread_trap)			\
    SYSCALL_ENTRY(self_yield)			\
    SYSCALL_ENTRY(self_halt)			\
    SYSCALL_ENTRY(self_id)			\
    SYSCALL_ENTRY(self_addref)			\
    SYSCALL_ENTRY(self_set_label)		\
    SYSCALL_ENTRY(self_set_clearance)		\
    SYSCALL_ENTRY(self_get_clearance)		\
    SYSCALL_ENTRY(self_set_verify)		\
    SYSCALL_ENTRY(self_get_verify)		\
    SYSCALL_ENTRY(self_get_as)			\
    SYSCALL_ENTRY(self_set_as)			\
    SYSCALL_ENTRY(self_enable_fp)		\
    SYSCALL_ENTRY(sync_wait)			\
    SYSCALL_ENTRY(sync_wakeup)			\
    SYSCALL_ENTRY(clock_msec)			\
    SYSCALL_ENTRY(pstate_timestamp)		\
    SYSCALL_ENTRY(pstate_sync)			\
    SYSCALL_ENTRY(segment_create)		\
    SYSCALL_ENTRY(segment_copy)			\
    SYSCALL_ENTRY(segment_addref)		\
    SYSCALL_ENTRY(segment_resize)		\
    SYSCALL_ENTRY(segment_get_nbytes)		\
    SYSCALL_ENTRY(segment_sync)			\
    SYSCALL_ENTRY(as_create)			\
    SYSCALL_ENTRY(as_get)			\
    SYSCALL_ENTRY(as_set)			\
    SYSCALL_ENTRY(as_set_slot)			\
    SYSCALL_ENTRY(mlt_create)			\
    SYSCALL_ENTRY(mlt_put)			\
    SYSCALL_ENTRY(mlt_get)			\

#define SYSCALL_ENTRY(name)	SYS_##name,

typedef enum {
    ALL_SYSCALLS
    NSYSCALLS
} syscall_num;

#undef SYSCALL_ENTRY

#endif
