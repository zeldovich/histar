#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

#define ALL_SYSCALLS \
    SYSCALL_ENTRY(cons_puts)			\
    SYSCALL_ENTRY(cons_getc)			\
    SYSCALL_ENTRY(cons_probe)			\
						\
    SYSCALL_ENTRY(fb_get_mode)			\
						\
    SYSCALL_ENTRY(device_create)		\
    SYSCALL_ENTRY(net_wait)			\
    SYSCALL_ENTRY(net_buf)			\
    SYSCALL_ENTRY(net_macaddr)			\
						\
    SYSCALL_ENTRY(machine_reboot)		\
						\
    SYSCALL_ENTRY(container_alloc)		\
    SYSCALL_ENTRY(container_get_nslots)		\
    SYSCALL_ENTRY(container_get_parent)		\
    SYSCALL_ENTRY(container_get_slot_id)	\
    SYSCALL_ENTRY(container_move_quota)		\
						\
    SYSCALL_ENTRY(obj_unref)			\
    SYSCALL_ENTRY(obj_get_type)			\
    SYSCALL_ENTRY(obj_get_label)		\
    SYSCALL_ENTRY(obj_get_name)			\
    SYSCALL_ENTRY(obj_get_quota_total)		\
    SYSCALL_ENTRY(obj_get_quota_avail)		\
    SYSCALL_ENTRY(obj_get_meta)			\
    SYSCALL_ENTRY(obj_set_meta)			\
    SYSCALL_ENTRY(obj_set_fixedquota)		\
    SYSCALL_ENTRY(obj_set_readonly)		\
    SYSCALL_ENTRY(obj_get_readonly)		\
    SYSCALL_ENTRY(obj_move)			\
    SYSCALL_ENTRY(obj_read)			\
    SYSCALL_ENTRY(obj_write)			\
    SYSCALL_ENTRY(obj_probe)			\
						\
    SYSCALL_ENTRY(gate_create)			\
    SYSCALL_ENTRY(gate_enter)			\
    SYSCALL_ENTRY(gate_clearance)		\
    SYSCALL_ENTRY(gate_get_entry)		\
						\
    SYSCALL_ENTRY(thread_create)		\
    SYSCALL_ENTRY(thread_start)			\
    SYSCALL_ENTRY(thread_trap)			\
						\
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
    SYSCALL_ENTRY(self_fp_enable)		\
    SYSCALL_ENTRY(self_fp_disable)		\
    SYSCALL_ENTRY(self_set_waitslots)		\
    SYSCALL_ENTRY(self_set_sched_parents)	\
    SYSCALL_ENTRY(self_set_cflush)		\
    SYSCALL_ENTRY(self_get_entry_args)		\
						\
    SYSCALL_ENTRY(sync_wait)			\
    SYSCALL_ENTRY(sync_wait_multi)		\
    SYSCALL_ENTRY(sync_wakeup)			\
    SYSCALL_ENTRY(clock_nsec)			\
    SYSCALL_ENTRY(handle_create)		\
    SYSCALL_ENTRY(pstate_timestamp)		\
    SYSCALL_ENTRY(pstate_sync)			\
						\
    SYSCALL_ENTRY(segment_create)		\
    SYSCALL_ENTRY(segment_copy)			\
    SYSCALL_ENTRY(segment_addref)		\
    SYSCALL_ENTRY(segment_resize)		\
    SYSCALL_ENTRY(segment_get_nbytes)		\
    SYSCALL_ENTRY(segment_sync)			\
						\
    SYSCALL_ENTRY(as_create)			\
    SYSCALL_ENTRY(as_copy)			\
    SYSCALL_ENTRY(as_get)			\
    SYSCALL_ENTRY(as_set)			\
    SYSCALL_ENTRY(as_get_slot)			\
    SYSCALL_ENTRY(as_set_slot)			\
						\
    SYSCALL_ENTRY(self_utrap_is_masked)		\
    SYSCALL_ENTRY(self_utrap_set_mask)		\
						\
    SYSCALL_ENTRY(jos_atomic_set)		\
    SYSCALL_ENTRY(jos_atomic_inc)		\
    SYSCALL_ENTRY(jos_atomic_dec)		\
    SYSCALL_ENTRY(jos_atomic_dec_and_test)	\
    SYSCALL_ENTRY(jos_atomic_compare_exchange)	\
						\
    SYSCALL_ENTRY(jos_atomic_set64)		\
    SYSCALL_ENTRY(jos_atomic_inc64)		\
    SYSCALL_ENTRY(jos_atomic_dec64)		\
    SYSCALL_ENTRY(jos_atomic_dec_and_test64)	\
    SYSCALL_ENTRY(jos_atomic_compare_exchange64)\
						\
    SYSCALL_ENTRY(irq_wait)


#ifndef __ASSEMBLER__
#define SYSCALL_ENTRY(name)	SYS_##name,

enum {
    ALL_SYSCALLS
    NSYSCALLS
};

#undef SYSCALL_ENTRY
#endif

#endif
