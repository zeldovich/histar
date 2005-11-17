#include <inc/syscall.h>
#include <inc/syscall_num.h>
#include <inc/types.h>

int
sys_cputs(const char *s)
{
    return syscall(SYS_cputs, (uint64_t) s, 0, 0, 0, 0);
}
