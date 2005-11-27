#include <inc/syscall.h>
#include <inc/syscall_num.h>
#include <inc/types.h>

#include <inc/stdio.h>

int
sys_cputs(const char *s)
{
    return syscall(SYS_cputs, (uint64_t) s, 0, 0, 0, 0);
}

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
sys_container_alloc(uint64_t parent)
{
    return syscall(SYS_container_alloc, parent, 0, 0, 0, 0);
}

int
sys_container_unref(uint64_t container, uint32_t idx)
{
    return syscall(SYS_container_unref, container, idx, 0, 0, 0);
}

int
sys_container_store_cur_thread(uint64_t container)
{
    return syscall(SYS_container_store_cur_thread, container, 0, 0, 0, 0);
}

int
sys_container_store_cur_addrspace(uint64_t container, int cow_data)
{
    return syscall(SYS_container_store_cur_addrspace, container, cow_data, 0, 0, 0);
}

container_object_type
sys_container_get_type(uint64_t container, uint32_t idx)
{
    return syscall(SYS_container_get_type, container, idx, 0, 0, 0);
}

int64_t
sys_container_get_c_idx(uint64_t container, uint32_t idx)
{
    return syscall(SYS_container_get_c_idx, container, idx, 0, 0, 0);
}

int
sys_gate_create(uint64_t container, void *entry, uint64_t arg, uint64_t as_ctr, uint32_t as_idx)
{
    return syscall(SYS_gate_create, container, (uint64_t) entry, arg, as_ctr, as_idx);
}

int
sys_gate_enter(uint64_t container, uint64_t idx)
{
    return syscall(SYS_gate_enter, container, idx, 0, 0, 0);
}

int
sys_thread_create(uint64_t container, uint64_t gt_ctr, uint32_t gt_idx)
{
    return syscall(SYS_thread_create, container, gt_ctr, gt_idx, 0, 0);
}
