#ifndef JOS_KERN_SYSCALL_H
#define JOS_KERN_SYSCALL_H

#include <inc/syscall_num.h>
#include <machine/types.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

#endif
