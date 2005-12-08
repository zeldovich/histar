#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/syscall_num.h>

int
sys_cons_puts(const char *s)
{
    return syscall(SYS_cons_puts, (uint64_t) s, 0, 0, 0, 0);
}

int
sys_cons_getc()
{
    return syscall(SYS_cons_getc, 0, 0, 0, 0, 0);
}

int64_t
sys_net_wait(int64_t waitgen)
{
    return syscall(SYS_net_wait, waitgen, 0, 0, 0, 0);
}

int
sys_net_buf(struct cobj_ref seg, uint64_t offset, netbuf_type type)
{
    return syscall(SYS_net_buf, seg.container, seg.slot, offset, type, 0);
}

int
sys_net_macaddr(uint8_t *addrbuf)
{
    return syscall(SYS_net_macaddr, (uint64_t) addrbuf, 0, 0, 0, 0);
}

int
sys_container_alloc(uint64_t parent)
{
    return syscall(SYS_container_alloc, parent, 0, 0, 0, 0);
}

int
sys_obj_unref(struct cobj_ref o)
{
    return syscall(SYS_obj_unref, o.container, o.slot, 0, 0, 0);
}

int
sys_container_store_cur_thread(uint64_t container)
{
    return syscall(SYS_container_store_cur_thread, container, 0, 0, 0, 0);
}

int64_t
sys_handle_create()
{
    return syscall(SYS_handle_create, 0, 0, 0, 0, 0);
}

kobject_type_t
sys_obj_get_type(struct cobj_ref o)
{
    return syscall(SYS_obj_get_type, o.container, o.slot, 0, 0, 0);
}

int64_t
sys_obj_get_id(struct cobj_ref o)
{
    return syscall(SYS_obj_get_id, o.container, o.slot, 0, 0, 0);
}

int
sys_obj_get_label(struct cobj_ref o, struct ulabel *l)
{
    return syscall(SYS_obj_get_label, o.container, o.slot, (uint64_t) l, 0, 0);
}

int
sys_container_nslots(uint64_t container)
{
    return syscall(SYS_container_nslots, container, 0, 0, 0, 0);
}

int
sys_gate_create(uint64_t container, struct thread_entry *te,
		struct ulabel *el, struct ulabel *tl)
{
    return syscall(SYS_gate_create, container, (uint64_t) te,
		   (uint64_t) el, (uint64_t) tl, 0);
}

int
sys_gate_enter(struct cobj_ref gate)
{
    return syscall(SYS_gate_enter, gate.container, gate.slot, 0, 0, 0);
}

int
sys_thread_create(uint64_t container)
{
    return syscall(SYS_thread_create, container, 0, 0, 0 ,0);
}

int
sys_thread_start(struct cobj_ref thread, struct thread_entry *entry)
{
    return syscall(SYS_thread_start, thread.container, thread.slot,
		   (uint64_t) entry, 0, 0);
}

void
sys_thread_yield()
{
    syscall(SYS_thread_yield, 0, 0, 0, 0, 0);
}

void
sys_thread_halt()
{
    syscall(SYS_thread_halt, 0, 0, 0, 0, 0);
}

int
sys_segment_create(uint64_t container, uint64_t num_pages)
{
    return syscall(SYS_segment_create, container, num_pages, 0, 0, 0);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_pages)
{
    return syscall(SYS_segment_resize, seg.container, seg.slot, num_pages,
		   0, 0);
}

int
sys_segment_get_npages(struct cobj_ref seg)
{
    return syscall(SYS_segment_get_npages, seg.container, seg.slot, 0, 0, 0);
}

int
sys_segment_get_map(struct segment_map *sm)
{
    return syscall(SYS_segment_get_map, (uint64_t) sm, 0, 0, 0, 0);
}
