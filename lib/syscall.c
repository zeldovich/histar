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
sys_thread_addref(uint64_t container)
{
    return syscall(SYS_thread_addref, container, 0, 0, 0, 0);
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
