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

uint64_t syscall(syscall_num num, ...);

int	sys_cons_puts(const char *s, uint64_t size);
int	sys_cons_getc(void);
int sys_cons_cursor(int line, int col);
int	sys_cons_probe(void);

int64_t sys_net_create(uint64_t container, struct ulabel *l, const char *name);
int64_t	sys_net_wait(struct cobj_ref ndev, uint64_t waiter_id,
		     int64_t waitgen);
int	sys_net_buf(struct cobj_ref ndev, struct cobj_ref seg,
		    uint64_t offset, netbuf_type type);
int	sys_net_macaddr(struct cobj_ref ndev, uint8_t *buf);

int64_t	sys_container_alloc(uint64_t parent, struct ulabel *l, const char *name);
int64_t	sys_container_nslots(uint64_t container);
int64_t	sys_container_get_slot_id(uint64_t container, uint64_t slot);

int	sys_obj_unref(struct cobj_ref o);
kobject_type_t
	sys_obj_get_type(struct cobj_ref o);
int	sys_obj_get_label(struct cobj_ref o, struct ulabel *l);
int	sys_obj_get_name(struct cobj_ref o, char *name);
int64_t	sys_obj_get_bytes(struct cobj_ref o);

int64_t	sys_handle_create(void);

int64_t	sys_gate_create(uint64_t container, struct thread_entry *s,
			struct ulabel *entry, struct ulabel *target,
			const char *name);
int	sys_gate_enter(struct cobj_ref gate,
		       struct ulabel *label,
		       struct ulabel *clearance);
int	sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul);

int64_t	sys_thread_create(uint64_t container, const char *name);
int	sys_thread_start(struct cobj_ref thread, struct thread_entry *s,
			 struct ulabel *l, struct ulabel *clearance);
int	sys_thread_trap(struct cobj_ref thread, uint32_t trapno, uint64_t arg);

void	sys_self_yield(void);
void	sys_self_halt(void);
int64_t sys_self_id(void);
int	sys_self_addref(uint64_t container);
int	sys_self_get_as(struct cobj_ref *as_obj);
int	sys_self_set_as(struct cobj_ref as_obj);
int	sys_self_set_label(struct ulabel *l);
int	sys_self_set_clearance(struct ulabel *l);
int	sys_self_get_clearance(struct ulabel *l);

int	sys_sync_wait(volatile uint64_t *addr, uint64_t val,
		      uint64_t wakeup_at_msec);
int	sys_sync_wakeup(volatile uint64_t *addr);

int64_t	sys_clock_msec(void);

int64_t	sys_segment_create(uint64_t container, uint64_t num_bytes,
			   struct ulabel *l, const char *name);
int64_t sys_segment_copy(struct cobj_ref seg, uint64_t container,
			 struct ulabel *l, const char *name);
int	sys_segment_addref(struct cobj_ref seg, uint64_t ct);
int	sys_segment_resize(struct cobj_ref seg, uint64_t num_bytes);
int64_t	sys_segment_get_nbytes(struct cobj_ref seg);

int64_t sys_as_create(uint64_t container, struct ulabel *l, const char *name);
int	sys_as_get(struct cobj_ref as, struct u_address_space *uas);
int	sys_as_set(struct cobj_ref as, struct u_address_space *uas);
int	sys_as_set_slot(struct cobj_ref as, struct u_segment_mapping *usm);

int64_t sys_mlt_create(uint64_t container, const char *name);
int	sys_mlt_put(struct cobj_ref mlt,
		    struct ulabel *l, uint8_t *buf, uint64_t *ct_id);
int	sys_mlt_get(struct cobj_ref mlt, uint64_t idx,
		    struct ulabel *l, uint8_t *buf, uint64_t *ct_id);

#endif
