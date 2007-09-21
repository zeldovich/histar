#include <inc/syscall_num.h>

.set sys_counter, 0
#define SYSCALL_ENTRY(name)     .set SYS_##name, sys_counter ; .set sys_counter, (sys_counter + 1) ;
ALL_SYSCALLS
#undef SYSCALL_ENTRY

