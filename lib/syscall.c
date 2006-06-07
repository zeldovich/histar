#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/syscall_num.h>

int
sys_cons_puts(const char *s, uint64_t size)
{
    return syscall(SYS_cons_puts, s, size);
}

int
sys_cons_getc(void)
{
    return syscall(SYS_cons_getc);
}

int
sys_cons_probe(void)
{
    return syscall(SYS_cons_probe);
}

int
sys_cons_cursor(int line, int col)
{
    return syscall(SYS_cons_cursor, line, col);
}

int64_t
sys_net_create(uint64_t container, struct ulabel *l, const char *name)
{
    return syscall(SYS_net_create, container, l, name);
}

int64_t
sys_net_wait(struct cobj_ref nd, uint64_t waiter_id, int64_t waitgen)
{
    return syscall(SYS_net_wait, nd, waiter_id, waitgen);
}

int
sys_net_buf(struct cobj_ref nd, struct cobj_ref seg, uint64_t offset,
	    netbuf_type type)
{
    return syscall(SYS_net_buf, nd, seg, offset, type);
}

int
sys_net_macaddr(struct cobj_ref nd, uint8_t *addrbuf)
{
    return syscall(SYS_net_macaddr, nd, addrbuf);
}

int64_t
sys_container_alloc(uint64_t parent, struct ulabel *ul, const char *name,
		    uint64_t avoid_types, uint64_t quota)
{
    return syscall(SYS_container_alloc, parent, ul, name, avoid_types, quota);
}

int
sys_obj_unref(struct cobj_ref o)
{
    return syscall(SYS_obj_unref, o);
}

int64_t
sys_container_get_parent(uint64_t container)
{
    return syscall(SYS_container_get_parent, container);
}

int64_t
sys_container_get_slot_id(uint64_t container, uint64_t slot)
{
    return syscall(SYS_container_get_slot_id, container, slot);
}

int
sys_container_move_quota(uint64_t parent, uint64_t child, int64_t nbytes)
{
    return syscall(SYS_container_move_quota, parent, child, nbytes);
}

int64_t
sys_handle_create(void)
{
    return syscall(SYS_handle_create);
}

kobject_type_t
sys_obj_get_type(struct cobj_ref o)
{
    return syscall(SYS_obj_get_type, o);
}

int
sys_obj_get_label(struct cobj_ref o, struct ulabel *l)
{
    return syscall(SYS_obj_get_label, o, l);
}

int
sys_obj_get_name(struct cobj_ref o, char *name)
{
    return syscall(SYS_obj_get_name, o, name);
}

int64_t
sys_obj_get_quota_total(struct cobj_ref o)
{
    return syscall(SYS_obj_get_quota_total, o);
}

int64_t
sys_obj_get_quota_avail(struct cobj_ref o)
{
    return syscall(SYS_obj_get_quota_avail, o);
}

int
sys_obj_get_meta(struct cobj_ref o, void *meta)
{
    return syscall(SYS_obj_get_meta, o, meta);
}

int
sys_obj_set_meta(struct cobj_ref o, const void *oldm, void *newm)
{
    return syscall(SYS_obj_set_meta, o, oldm, newm);
}

int
sys_obj_set_fixedquota(struct cobj_ref o)
{
    return syscall(SYS_obj_set_fixedquota, o);
}

int
sys_obj_set_readonly(struct cobj_ref o)
{
    return syscall(SYS_obj_set_readonly, o);
}

int
sys_obj_get_readonly(struct cobj_ref o)
{
    return syscall(SYS_obj_get_readonly, o);
}

int64_t
sys_container_get_nslots(uint64_t container)
{
    return syscall(SYS_container_get_nslots, container);
}

int64_t
sys_gate_create(uint64_t container, struct thread_entry *te,
		struct ulabel *el, struct ulabel *tl,
		const char *name, int entry_visible)
{
    return syscall(SYS_gate_create, container, te, el, tl, name, entry_visible);
}

int
sys_gate_enter(struct cobj_ref gate,
	       struct ulabel *l,
	       struct ulabel *clearance)
{
    return syscall(SYS_gate_enter, gate, l, clearance);
}

int
sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul)
{
    return syscall(SYS_gate_clearance, gate, ul);
}

int
sys_gate_get_entry(struct cobj_ref gate, struct thread_entry *s)
{
    return syscall(SYS_gate_get_entry, gate, s);
}

int64_t
sys_thread_create(uint64_t container, const char *name)
{
    return syscall(SYS_thread_create, container, name);
}

int
sys_thread_start(struct cobj_ref thread, struct thread_entry *entry,
		 struct ulabel *ul, struct ulabel *clearance)
{
    return syscall(SYS_thread_start, thread, entry, ul, clearance);
}

int
sys_thread_trap(struct cobj_ref thread, struct cobj_ref as,
		uint32_t trapno, uint64_t arg)
{
    return syscall(SYS_thread_trap, thread, as, trapno, arg);
}

void
sys_self_yield(void)
{
    syscall(SYS_self_yield);
}

void
sys_self_halt(void)
{
    syscall(SYS_self_halt);
}

int64_t
sys_self_id(void)
{
    return syscall(SYS_self_id);
}

int
sys_self_addref(uint64_t container)
{
    return syscall(SYS_self_addref, container);
}

int
sys_self_get_as(struct cobj_ref *as_obj)
{
    return syscall(SYS_self_get_as, as_obj);
}

int
sys_self_set_as(struct cobj_ref as_obj)
{
    return syscall(SYS_self_set_as, as_obj);
}

int
sys_self_set_label(struct ulabel *l)
{
    return syscall(SYS_self_set_label, l);
}

int
sys_self_set_clearance(struct ulabel *l)
{
    return syscall(SYS_self_set_clearance, l);
}

int
sys_self_get_clearance(struct ulabel *l)
{
    return syscall(SYS_self_get_clearance, l);
}

int
sys_self_set_verify(struct ulabel *l)
{
    return syscall(SYS_self_set_verify, l);
}

int
sys_self_get_verify(struct ulabel *l)
{
    return syscall(SYS_self_get_verify, l);
}

int
sys_self_fp_enable(void)
{
    return syscall(SYS_self_fp_enable);
}

int
sys_self_fp_disable(void)
{
    return syscall(SYS_self_fp_disable);
}

int
sys_sync_wait(volatile uint64_t *addr, uint64_t val, uint64_t msec)
{
    return syscall(SYS_sync_wait, addr, val, msec);
}

int
sys_sync_wakeup(volatile uint64_t *addr)
{
    return syscall(SYS_sync_wakeup, addr);
}

int64_t
sys_clock_msec(void)
{
    return syscall(SYS_clock_msec);
}

int64_t
sys_pstate_timestamp(void)
{
    return syscall(SYS_pstate_timestamp);
}

int
sys_pstate_sync(uint64_t timestamp)
{
    return syscall(SYS_pstate_sync, timestamp);
}

int64_t
sys_segment_create(uint64_t container, uint64_t num_bytes,
		   struct ulabel *l, const char *name)
{
    return syscall(SYS_segment_create, container, num_bytes, l, name);
}

int64_t
sys_segment_copy(struct cobj_ref seg, uint64_t container,
		 struct ulabel *l, const char *name)
{
    return syscall(SYS_segment_copy, seg, container, l, name);
}

int
sys_segment_addref(struct cobj_ref seg, uint64_t ct)
{
    return syscall(SYS_segment_addref, seg, ct);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_bytes)
{
    return syscall(SYS_segment_resize, seg, num_bytes);
}

int64_t
sys_segment_get_nbytes(struct cobj_ref seg)
{
    return syscall(SYS_segment_get_nbytes, seg);
}

int
sys_segment_sync(struct cobj_ref seg, uint64_t start, uint64_t nbytes, uint64_t pstate_ts)
{
    return syscall(SYS_segment_sync, seg, start, nbytes, pstate_ts);
}

int64_t
sys_as_create(uint64_t container, struct ulabel *l, const char *name)
{
    return syscall(SYS_as_create, container, l, name);
}

int
sys_as_get(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_get, as, uas);
}

int
sys_as_set(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_set, as, uas);
}

int
sys_as_set_slot(struct cobj_ref as, struct u_segment_mapping *usm)
{
    return syscall(SYS_as_set_slot, as, usm);
}
