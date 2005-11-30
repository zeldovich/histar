#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <inc/types.h>
#include <inc/syscall_num.h>
#include <inc/container.h>
#include <inc/segment.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

void	sys_yield();
void	sys_halt();

int	sys_cputs(const char *s);
int	sys_cgetc();

int	sys_container_alloc(uint64_t parent);
int	sys_container_unref(struct cobj_ref o);
container_object_type
	sys_container_get_type(struct cobj_ref o);
int64_t	sys_container_get_c_idx(struct cobj_ref o);
int	sys_container_store_cur_thread(uint64_t container);
int	sys_container_store_cur_pmap(uint64_t container, int copy);

int	sys_gate_create(uint64_t container, void *entry, void *stack,
		struct cobj_ref pmap, int pmap_copy, uint64_t arg);
int	sys_gate_enter(struct cobj_ref gate);

int	sys_thread_create(uint64_t container, struct cobj_ref gate);

int	sys_pmap_create(uint64_t container);
int	sys_pmap_unmap(struct cobj_ref pmap, void *start, uint64_t num_pages);

int	sys_segment_create(uint64_t container, uint64_t num_pages);
int	sys_segment_resize(struct cobj_ref seg, uint64_t num_pages);
int	sys_segment_get_npages(struct cobj_ref seg);
int	sys_segment_map(struct cobj_ref seg, struct cobj_ref pmap, void *va,
		uint64_t start_page, uint64_t num_pages, segment_map_mode mode);

#endif
