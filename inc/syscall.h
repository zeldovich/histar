#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <inc/types.h>
#include <inc/syscall_num.h>
#include <inc/container.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

int	sys_cputs(const char *s);
void	sys_yield();
void	sys_halt();

int	sys_container_alloc(uint64_t parent);
int	sys_container_unref(uint64_t container, uint32_t idx);
container_object_type
	sys_container_get_type(uint64_t container, uint32_t idx);
int64_t	sys_container_get_c_idx(uint64_t container, uint32_t idx);
int	sys_container_store_cur_thread(uint64_t container);
int	sys_container_store_cur_addrspace(uint64_t container);

#endif
