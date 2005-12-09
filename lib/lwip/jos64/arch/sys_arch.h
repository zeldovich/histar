#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include <inc/types.h>

typedef	int sys_sem_t;
typedef int sys_mbox_t;
typedef void* sys_thread_t;

#define SYS_MBOX_NULL	(-1)
#define SYS_SEM_NULL	(-1)

#endif
