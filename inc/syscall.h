#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <inc/types.h>
#include <inc/syscall_num.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

int	sys_cputs(const char *s);
void	sys_yield();
void	sys_halt();

#endif
