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
int	sys_container_unref(struct cobj_ref o);
container_object_type
	sys_container_get_type(struct cobj_ref o);
int64_t	sys_container_get_c_idx(struct cobj_ref o);
int	sys_container_store_cur_thread(uint64_t container);
int	sys_container_store_cur_addrspace(uint64_t container, int cow_data);

int	sys_gate_create(uint64_t container, void *entry, uint64_t arg, struct cobj_ref as);
int	sys_gate_enter(struct cobj_ref gate);

int	sys_thread_create(uint64_t container, struct cobj_ref gate);

#endif
