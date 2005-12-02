#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/syscall_num.h>

void
sys_yield()
{
    syscall(SYS_yield, 0, 0, 0, 0, 0);
}

void
sys_halt()
{
    syscall(SYS_halt, 0, 0, 0, 0, 0);
}

int
sys_cputs(const char *s)
{
    return syscall(SYS_cputs, (uint64_t) s, 0, 0, 0, 0);
}

int
sys_cgetc()
{
    return syscall(SYS_cgetc, 0, 0, 0, 0, 0);
}

int
sys_container_alloc(uint64_t parent)
{
    return syscall(SYS_container_alloc, parent, 0, 0, 0, 0);
}

int
sys_container_unref(struct cobj_ref o)
{
    return syscall(SYS_container_unref, o.container, o.slot, 0, 0, 0);
}

int
sys_container_store_cur_thread(uint64_t container)
{
    return syscall(SYS_container_store_cur_thread, container, 0, 0, 0, 0);
}

kobject_type_t
sys_container_get_type(struct cobj_ref o)
{
    return syscall(SYS_container_get_type, o.container, o.slot, 0, 0, 0);
}

int64_t
sys_container_get_c_id(struct cobj_ref o)
{
    return syscall(SYS_container_get_c_id, o.container, o.slot, 0, 0, 0);
}

int
sys_gate_create(uint64_t container, struct thread_entry *entry)
{
    return syscall(SYS_gate_create, container, (uint64_t) entry, 0, 0, 0);
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
    return syscall(SYS_thread_start, thread.container, thread.slot, (uint64_t) entry, 0, 0);
}

int
sys_segment_create(uint64_t container, uint64_t num_pages)
{
    return syscall(SYS_segment_create, container, num_pages, 0, 0, 0);
}

int
sys_segment_resize(struct cobj_ref seg, uint64_t num_pages)
{
    return syscall(SYS_segment_resize, seg.container, seg.slot, num_pages, 0, 0);
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
