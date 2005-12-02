#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <inc/types.h>
#include <inc/syscall_num.h>
#include <inc/container.h>
#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/kobj.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

void	sys_yield();
void	sys_halt();

int	sys_cputs(const char *s);
int	sys_cgetc();

int	sys_container_alloc(uint64_t parent);
int	sys_container_unref(struct cobj_ref o);
kobject_type_t
	sys_container_get_type(struct cobj_ref o);
int64_t	sys_container_get_c_id(struct cobj_ref o);
int	sys_container_store_cur_thread(uint64_t container);

int	sys_gate_create(uint64_t container, struct thread_entry *s);
int	sys_gate_enter(struct cobj_ref gate);

int	sys_thread_create(uint64_t container);
int	sys_thread_start(struct cobj_ref thread, struct thread_entry *s);

int	sys_segment_create(uint64_t container, uint64_t num_pages);
int	sys_segment_resize(struct cobj_ref seg, uint64_t num_pages);
int	sys_segment_get_npages(struct cobj_ref seg);
int	sys_segment_get_map(struct segment_map *sm);

#endif
