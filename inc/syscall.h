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
int	sys_cons_cursor(int line, int col);
int	sys_cons_probe(void);

int64_t sys_net_create(uint64_t container, struct ulabel *l, const char *name);
int64_t	sys_net_wait(struct cobj_ref ndev, uint64_t waiter_id,
		     int64_t waitgen);
int	sys_net_buf(struct cobj_ref ndev, struct cobj_ref seg,
		    uint64_t offset, netbuf_type type);
int	sys_net_macaddr(struct cobj_ref ndev, uint8_t *buf);

int64_t	sys_container_alloc(uint64_t parent, struct ulabel *l, const char *name,
			    uint64_t avoid_types, uint64_t quota);
int64_t	sys_container_get_nslots(uint64_t container);
int64_t sys_container_get_parent(uint64_t container);
int64_t	sys_container_get_slot_id(uint64_t container, uint64_t slot);
int	sys_container_move_quota(uint64_t parent, uint64_t child, int64_t nbytes);

int	sys_obj_unref(struct cobj_ref o);
kobject_type_t
	sys_obj_get_type(struct cobj_ref o);
int	sys_obj_get_label(struct cobj_ref o, struct ulabel *l);
int	sys_obj_get_name(struct cobj_ref o, char *name);
int64_t	sys_obj_get_quota_total(struct cobj_ref o);
int64_t sys_obj_get_quota_avail(struct cobj_ref o);
int	sys_obj_get_meta(struct cobj_ref o, void *meta);
int	sys_obj_set_meta(struct cobj_ref o, const void *oldm, void *newm);
int	sys_obj_set_fixedquota(struct cobj_ref o);
int	sys_obj_set_readonly(struct cobj_ref o);
int	sys_obj_get_readonly(struct cobj_ref o);

int64_t	sys_handle_create(void);

int64_t	sys_gate_create(uint64_t container, struct thread_entry *s,
			struct ulabel *entry, struct ulabel *target,
			const char *name, int entry_visible);
int	sys_gate_enter(struct cobj_ref gate,
		       struct ulabel *label,
		       struct ulabel *clearance);
int	sys_gate_clearance(struct cobj_ref gate, struct ulabel *ul);
int	sys_gate_get_entry(struct cobj_ref gate, struct thread_entry *s);

int64_t	sys_thread_create(uint64_t container, const char *name);
int	sys_thread_start(struct cobj_ref thread, struct thread_entry *s,
			 struct ulabel *l, struct ulabel *clearance);
int	sys_thread_trap(struct cobj_ref thread, struct cobj_ref as,
			uint32_t trapno, uint64_t arg);

void	sys_self_yield(void);
void	sys_self_halt(void);
int64_t sys_self_id(void);
int	sys_self_addref(uint64_t container);
int	sys_self_get_as(struct cobj_ref *as_obj);
int	sys_self_set_as(struct cobj_ref as_obj);
int	sys_self_set_label(struct ulabel *l);
int	sys_self_set_clearance(struct ulabel *l);
int	sys_self_get_clearance(struct ulabel *l);
int	sys_self_set_verify(struct ulabel *l);
int	sys_self_get_verify(struct ulabel *l);
int	sys_self_fp_enable(void);
int	sys_self_fp_disable(void);

int	sys_sync_wait(volatile uint64_t *addr, uint64_t val,
		      uint64_t wakeup_at_msec);
int	sys_sync_wakeup(volatile uint64_t *addr);

int64_t	sys_clock_msec(void);

int64_t	sys_pstate_timestamp(void);
int	sys_pstate_sync(uint64_t timestamp);

int64_t	sys_segment_create(uint64_t container, uint64_t num_bytes,
			   struct ulabel *l, const char *name);
int64_t sys_segment_copy(struct cobj_ref seg, uint64_t container,
			 struct ulabel *l, const char *name);
int	sys_segment_addref(struct cobj_ref seg, uint64_t ct);
int	sys_segment_resize(struct cobj_ref seg, uint64_t num_bytes);
int64_t	sys_segment_get_nbytes(struct cobj_ref seg);
int	sys_segment_sync(struct cobj_ref seg, uint64_t start, uint64_t nbytes,
			 uint64_t pstate_ts);

int64_t sys_as_create(uint64_t container, struct ulabel *l, const char *name);
int	sys_as_get(struct cobj_ref as, struct u_address_space *uas);
int	sys_as_set(struct cobj_ref as, struct u_address_space *uas);
int	sys_as_set_slot(struct cobj_ref as, struct u_segment_mapping *usm);

#endif
