#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/syscall_num.h>

int
sys_cons_puts(const char *s, uint64_t size)
{
    return syscall(SYS_cons_puts, (uint64_t) s, size, 0, 0, 0);
}

int
sys_cons_getc(void)
{
    return syscall(SYS_cons_getc, 0, 0, 0, 0, 0);
}

int64_t
sys_net_wait(uint64_t waiter_id, int64_t waitgen)
{
    return syscall(SYS_net_wait, waiter_id, waitgen, 0, 0, 0);
}

int
sys_net_buf(struct cobj_ref seg, uint64_t offset, netbuf_type type)
{
    return syscall(SYS_net_buf, seg.container, seg.object, offset, type, 0);
}

int
sys_net_macaddr(uint8_t *addrbuf)
{
    return syscall(SYS_net_macaddr, (uint64_t) addrbuf, 0, 0, 0, 0);
}

int64_t
sys_container_alloc(uint64_t parent)
{
    return syscall(SYS_container_alloc, parent, 0, 0, 0, 0);
}

int
sys_obj_unref(struct cobj_ref o)
{
    return syscall(SYS_obj_unref, o.container, o.object, 0, 0, 0);
}

int64_t
sys_container_get_slot_id(uint64_t container, uint64_t slot)
{
    return syscall(SYS_container_get_slot_id, container, slot, 0, 0, 0);
}

int64_t
sys_handle_create(void)
{
    return syscall(SYS_handle_create, 0, 0, 0, 0, 0);
}

kobject_type_t
sys_obj_get_type(struct cobj_ref o)
{
    return syscall(SYS_obj_get_type, o.container, o.object, 0, 0, 0);
}

int
sys_obj_get_label(struct cobj_ref o, struct ulabel *l)
{
    return syscall(SYS_obj_get_label, o.container, o.object, (uint64_t) l, 0, 0);
}

int64_t
sys_container_nslots(uint64_t container)
{
    return syscall(SYS_container_nslots, container, 0, 0, 0, 0);
}

int64_t
sys_gate_create(uint64_t container, struct thread_entry *te,
		struct ulabel *el, struct ulabel *tl)
{
    return syscall(SYS_gate_create, container, (uint64_t) te,
		   (uint64_t) el, (uint64_t) tl, 0);
}

int
sys_gate_enter(struct cobj_ref gate, uint64_t a1, uint64_t a2)
{
    return syscall(SYS_gate_enter, gate.container, gate.object, a1, a2, 0);
}

int64_t
sys_thread_create(uint64_t container)
{
    return syscall(SYS_thread_create, container, 0, 0, 0 ,0);
}

int
sys_thread_start(struct cobj_ref thread, struct thread_entry *entry)
{
    return syscall(SYS_thread_start, thread.container, thread.object,
		   (uint64_t) entry, 0, 0);
}

void
sys_thread_yield(void)
{
    syscall(SYS_thread_yield, 0, 0, 0, 0, 0);
}

void
sys_thread_halt(void)
{
    syscall(SYS_thread_halt, 0, 0, 0, 0, 0);
}

void
sys_thread_sleep(uint64_t msec)
{
    syscall(SYS_thread_sleep, msec, 0, 0, 0, 0);
}

int64_t
sys_thread_id(void)
{
    return syscall(SYS_thread_id, 0, 0, 0, 0, 0);
}

int
sys_thread_addref(uint64_t container)
{
    return syscall(SYS_thread_addref, container, 0, 0, 0, 0);
}

int
sys_thread_get_as(struct cobj_ref *as_obj)
{
    return syscall(SYS_thread_get_as, (uint64_t) as_obj, 0, 0, 0, 0);
}

int64_t
sys_segment_create(uint64_t container, uint64_t num_pages)
{
    return syscall(SYS_segment_create, container, num_pages, 0, 0, 0);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_pages)
{
    return syscall(SYS_segment_resize, seg.container, seg.object, num_pages,
		   0, 0);
}

int64_t
sys_segment_get_npages(struct cobj_ref seg)
{
    return syscall(SYS_segment_get_npages, seg.container, seg.object, 0, 0, 0);
}

int64_t
sys_as_create(uint64_t container)
{
    return syscall(SYS_as_create, container, 0, 0, 0, 0);
}

int
sys_as_get(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_get, as.container, as.object, (uint64_t) uas, 0, 0);
}

int
sys_as_set(struct cobj_ref as, struct u_address_space *uas)
{
    return syscall(SYS_as_set, as.container, as.object, (uint64_t) uas, 0, 0);
}
