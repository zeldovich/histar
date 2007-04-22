#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/syscall_num.h>

#define SOBJ(obj)	(obj).container, (obj).object
#define SPTR(ptr)	((uint64_t) (uintptr_t) (ptr))

int
sys_cons_puts(const char *s, uint64_t size)
{
    return syscall(SYS_cons_puts, SPTR(s), size, 0, 0, 0, 0, 0);
}

int
sys_cons_getc(void)
{
    return syscall(SYS_cons_getc, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_cons_probe(void)
{
    return syscall(SYS_cons_probe, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_cons_cursor(int line, int col)
{
    return syscall(SYS_cons_cursor, line, col, 0, 0, 0, 0, 0);
}

int64_t
sys_net_create(uint64_t container, uint64_t card_idx,
	       const struct ulabel *l, const char *name)
{
    return syscall(SYS_net_create, container, card_idx, SPTR(l), SPTR(name),
		   0, 0, 0);
}

int64_t
sys_net_wait(struct cobj_ref nd, uint64_t waiter_id, int64_t waitgen)
{
    return syscall(SYS_net_wait, SOBJ(nd), waiter_id, waitgen, 0, 0, 0);
}

int
sys_net_buf(struct cobj_ref nd, struct cobj_ref seg, uint64_t offset,
	    netbuf_type type)
{
    return syscall(SYS_net_buf, SOBJ(nd), SOBJ(seg), offset, type, 0);
}

int
sys_net_macaddr(struct cobj_ref nd, uint8_t *addrbuf)
{
    return syscall(SYS_net_macaddr, SOBJ(nd), SPTR(addrbuf), 0, 0, 0, 0);
}

int
sys_machine_reboot(void)
{
    return syscall(SYS_machine_reboot, 0, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_container_alloc(uint64_t parent, const struct ulabel *ul, const char *name,
		    uint64_t avoid_types, uint64_t quota)
{
    return syscall(SYS_container_alloc, parent, SPTR(ul), SPTR(name),
		   avoid_types, quota, 0, 0);
}

int
sys_obj_unref(struct cobj_ref o)
{
    return syscall(SYS_obj_unref, SOBJ(o), 0, 0, 0, 0, 0);
}

int64_t
sys_container_get_parent(uint64_t container)
{
    return syscall(SYS_container_get_parent, container, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_container_get_slot_id(uint64_t container, uint64_t slot)
{
    return syscall(SYS_container_get_slot_id, container, slot, 0, 0, 0, 0, 0);
}

int
sys_container_move_quota(uint64_t parent, uint64_t child, int64_t nbytes)
{
    return syscall(SYS_container_move_quota, parent, child, nbytes, 0, 0, 0, 0);
}

int64_t
sys_handle_create(void)
{
    return syscall(SYS_handle_create, 0, 0, 0, 0, 0, 0, 0);
}

kobject_type_t
sys_obj_get_type(struct cobj_ref o)
{
    return syscall(SYS_obj_get_type, SOBJ(o), 0, 0, 0, 0, 0);
}

int
sys_obj_get_label(struct cobj_ref o, struct ulabel *l)
{
    return syscall(SYS_obj_get_label, SOBJ(o), SPTR(l), 0, 0, 0, 0);
}

int
sys_obj_get_name(struct cobj_ref o, char *name)
{
    return syscall(SYS_obj_get_name, SOBJ(o), SPTR(name), 0, 0, 0, 0);
}

int64_t
sys_obj_get_quota_total(struct cobj_ref o)
{
    return syscall(SYS_obj_get_quota_total, SOBJ(o), 0, 0, 0, 0, 0);
}

int64_t
sys_obj_get_quota_avail(struct cobj_ref o)
{
    return syscall(SYS_obj_get_quota_avail, SOBJ(o), 0, 0, 0, 0, 0);
}

int
sys_obj_get_meta(struct cobj_ref o, void *meta)
{
    return syscall(SYS_obj_get_meta, SOBJ(o), SPTR(meta), 0, 0, 0, 0);
}

int
sys_obj_set_meta(struct cobj_ref o, const void *oldm, void *newm)
{
    return syscall(SYS_obj_set_meta, SOBJ(o), SPTR(oldm), SPTR(newm), 0, 0, 0);
}

int
sys_obj_set_fixedquota(struct cobj_ref o)
{
    return syscall(SYS_obj_set_fixedquota, SOBJ(o), 0, 0, 0, 0, 0);
}

int
sys_obj_set_readonly(struct cobj_ref o)
{
    return syscall(SYS_obj_set_readonly, SOBJ(o), 0, 0, 0, 0, 0);
}

int
sys_obj_get_readonly(struct cobj_ref o)
{
    return syscall(SYS_obj_get_readonly, SOBJ(o), 0, 0, 0, 0, 0);
}

int64_t
sys_container_get_nslots(uint64_t container)
{
    return syscall(SYS_container_get_nslots, container, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_gate_create(uint64_t container, const struct thread_entry *te,
		const struct ulabel *label, const struct ulabel *clear,
		const struct ulabel *verify, const char *name,
		int entry_visible)
{
    return syscall(SYS_gate_create, container, SPTR(te),
		   SPTR(label), SPTR(clear), SPTR(verify),
		   SPTR(name), entry_visible);
}

int
sys_gate_enter(struct cobj_ref gate,
	       const struct ulabel *l,
	       const struct ulabel *clearance,
	       const struct thread_entry *te)
{
    return syscall(SYS_gate_enter, SOBJ(gate), SPTR(l), SPTR(clearance),
		   SPTR(te), 0, 0);
}

int
sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul)
{
    return syscall(SYS_gate_clearance, SOBJ(gate), SPTR(ul), 0, 0, 0, 0);
}

int
sys_gate_get_entry(struct cobj_ref gate, struct thread_entry *s)
{
    return syscall(SYS_gate_get_entry, SOBJ(gate), SPTR(s), 0, 0, 0, 0);
}

int64_t
sys_thread_create(uint64_t container, const char *name)
{
    return syscall(SYS_thread_create, container, SPTR(name), 0, 0, 0, 0, 0);
}

int
sys_thread_start(struct cobj_ref thread, const struct thread_entry *entry,
		 const struct ulabel *ul, const struct ulabel *clearance)
{
    return syscall(SYS_thread_start, SOBJ(thread), SPTR(entry),
		   SPTR(ul), SPTR(clearance), 0, 0);
}

int
sys_thread_trap(struct cobj_ref thread, struct cobj_ref as,
		uint32_t trapno, uint64_t arg)
{
    return syscall(SYS_thread_trap, SOBJ(thread), SOBJ(as), trapno, arg, 0);
}

void
sys_self_yield(void)
{
    syscall(SYS_self_yield, 0, 0, 0, 0, 0, 0, 0);
}

void
sys_self_halt(void)
{
    syscall(SYS_self_halt, 0, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_self_id(void)
{
    return syscall(SYS_self_id, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_self_addref(uint64_t container)
{
    return syscall(SYS_self_addref, container, 0, 0, 0, 0, 0, 0);
}

int
sys_self_get_as(struct cobj_ref *as_obj)
{
    return syscall(SYS_self_get_as, SPTR(as_obj), 0, 0, 0, 0, 0, 0);
}

int
sys_self_set_as(struct cobj_ref as_obj)
{
    return syscall(SYS_self_set_as, SOBJ(as_obj), 0, 0, 0, 0, 0);
}

int
sys_self_set_label(const struct ulabel *l)
{
    return syscall(SYS_self_set_label, SPTR(l), 0, 0, 0, 0, 0, 0);
}

int
sys_self_set_clearance(const struct ulabel *l)
{
    return syscall(SYS_self_set_clearance, SPTR(l), 0, 0, 0, 0, 0, 0);
}

int
sys_self_get_clearance(struct ulabel *l)
{
    return syscall(SYS_self_get_clearance, SPTR(l), 0, 0, 0, 0, 0, 0);
}

int
sys_self_set_verify(const struct ulabel *l, const struct ulabel *c)
{
    return syscall(SYS_self_set_verify, SPTR(l), SPTR(c), 0, 0, 0, 0, 0);
}

int
sys_self_get_verify(struct ulabel *l, struct ulabel *c)
{
    return syscall(SYS_self_get_verify, SPTR(l), SPTR(c), 0, 0, 0, 0, 0);
}

int
sys_self_fp_enable(void)
{
    return syscall(SYS_self_fp_enable, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_self_fp_disable(void)
{
    return syscall(SYS_self_fp_disable, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_self_set_waitslots(uint64_t nslots)
{
    return syscall(SYS_self_set_waitslots, nslots, 0, 0, 0, 0, 0, 0);
}

int
sys_self_set_sched_parents(uint64_t p0, uint64_t p1)
{
    return syscall(SYS_self_set_sched_parents, p0, p1, 0, 0, 0, 0, 0);
}

int
sys_self_set_cflush(int cflush)
{
    return syscall(SYS_self_set_cflush, cflush, 0, 0, 0, 0, 0, 0);
}

int
sys_self_get_entry_args(struct thread_entry_args *targ)
{
    return syscall(SYS_self_get_entry_args, SPTR(targ), 0, 0, 0, 0, 0, 0);
}

int
sys_sync_wait(volatile uint64_t *addr, uint64_t val, uint64_t nsec)
{
    return syscall(SYS_sync_wait, SPTR(addr), val, nsec, 0, 0, 0, 0);
}

int 
sys_sync_wait_multi(volatile uint64_t **addrs, uint64_t *vals,
		    uint64_t num, uint64_t nsec)
{
    return syscall(SYS_sync_wait_multi, SPTR(addrs), SPTR(vals), num, nsec,
		   0, 0, 0);
}

int
sys_sync_wakeup(volatile uint64_t *addr)
{
    return syscall(SYS_sync_wakeup, SPTR(addr), 0, 0, 0, 0, 0, 0);
}

int64_t
sys_clock_nsec(void)
{
    return syscall(SYS_clock_nsec, 0, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_pstate_timestamp(void)
{
    return syscall(SYS_pstate_timestamp, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_pstate_sync(uint64_t timestamp)
{
    return syscall(SYS_pstate_sync, timestamp, 0, 0, 0, 0, 0, 0);
}

int64_t
sys_segment_create(uint64_t container, uint64_t num_bytes,
		   const struct ulabel *l, const char *name)
{
    return syscall(SYS_segment_create, container, num_bytes,
		   SPTR(l), SPTR(name), 0, 0, 0);
}

int64_t
sys_segment_copy(struct cobj_ref seg, uint64_t container,
		 const struct ulabel *l, const char *name)
{
    return syscall(SYS_segment_copy, SOBJ(seg), container,
		   SPTR(l), SPTR(name), 0, 0);
}

int
sys_segment_addref(struct cobj_ref seg, uint64_t ct)
{
    return syscall(SYS_segment_addref, SOBJ(seg), ct, 0, 0, 0, 0);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_bytes)
{
    return syscall(SYS_segment_resize, SOBJ(seg), num_bytes, 0, 0, 0, 0);
}

int64_t
sys_segment_get_nbytes(struct cobj_ref seg)
{
    return syscall(SYS_segment_get_nbytes, SOBJ(seg), 0, 0, 0, 0, 0);
}

int
sys_segment_sync(struct cobj_ref seg, uint64_t start, uint64_t nbytes, uint64_t pstate_ts)
{
    return syscall(SYS_segment_sync, SOBJ(seg), start, nbytes, pstate_ts, 0, 0);
}

int64_t
sys_as_create(uint64_t container, const struct ulabel *l, const char *name)
{
    return syscall(SYS_as_create, container, SPTR(l), SPTR(name), 0, 0, 0, 0);
}

int64_t
sys_as_copy(struct cobj_ref as, uint64_t container,
	    const struct ulabel *l, const char *name)
{
    return syscall(SYS_as_copy, SOBJ(as), container, SPTR(l), SPTR(name), 0, 0);
}

int
sys_as_get(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_get, SOBJ(as), SPTR(uas), 0, 0, 0, 0);
}

int
sys_as_set(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_set, SOBJ(as), SPTR(uas), 0, 0, 0, 0);
}

int
sys_as_get_slot(struct cobj_ref as, struct u_segment_mapping *usm)
{
    return syscall(SYS_as_get_slot, SOBJ(as), SPTR(usm), 0, 0, 0, 0);
}

int
sys_as_set_slot(struct cobj_ref as, struct u_segment_mapping *usm)
{
    return syscall(SYS_as_set_slot, SOBJ(as), SPTR(usm), 0, 0, 0, 0);
}
