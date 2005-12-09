#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#include <inc/types.h>
#include <inc/syscall_num.h>
#include <inc/container.h>
#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/kobj.h>
#include <inc/label.h>
#include <inc/netdev.h>

uint64_t syscall(syscall_num num, uint64_t a1, uint64_t a2,
		 uint64_t a3, uint64_t a4, uint64_t a5);

int	sys_cons_puts(const char *s);
int	sys_cons_getc();

int64_t	sys_net_wait(uint64_t waiter_id, int64_t waitgen);
int	sys_net_buf(struct cobj_ref seg, uint64_t offset, netbuf_type type);
int	sys_net_macaddr(uint8_t *buf);

int	sys_container_alloc(uint64_t parent);
int	sys_container_store_cur_thread(uint64_t container);
int	sys_container_nslots(uint64_t container);

int	sys_obj_unref(struct cobj_ref o);
kobject_type_t
	sys_obj_get_type(struct cobj_ref o);
int64_t	sys_obj_get_id(struct cobj_ref o);
int	sys_obj_get_label(struct cobj_ref o, struct ulabel *l);

int64_t	sys_handle_create();

int	sys_gate_create(uint64_t container, struct thread_entry *s,
			struct ulabel *entry, struct ulabel *target);
int	sys_gate_enter(struct cobj_ref gate);

int	sys_thread_create(uint64_t container);
int	sys_thread_start(struct cobj_ref thread, struct thread_entry *s);
void	sys_thread_yield();
void	sys_thread_halt();
void	sys_thread_sleep(uint64_t msec);

int	sys_segment_create(uint64_t container, uint64_t num_pages);
int	sys_segment_resize(struct cobj_ref seg, uint64_t num_pages);
int	sys_segment_get_npages(struct cobj_ref seg);
int	sys_segment_get_map(struct segment_map *sm);

#endif
