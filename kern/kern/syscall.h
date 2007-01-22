#ifndef JOS_KERN_SYSCALL_H
#define JOS_KERN_SYSCALL_H

#include <machine/types.h>
#include <inc/syscall_num.h>

uint64_t kern_syscall(syscall_num num, uint64_t a1,
		      uint64_t a2, uint64_t a3, uint64_t a4,
		      uint64_t a5, uint64_t a6, uint64_t a7);

#endif
