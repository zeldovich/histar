#ifndef JOS_KERN_STACKWRAP_H
#define JOS_KERN_STACKWRAP_H

#include <machine/types.h>
#include <kern/disk.h>
#include <kern/part.h>
#include <inc/queue.h>

typedef void (*stackwrap_fn) (uint64_t, uint64_t, uint64_t);

int  stackwrap_call(stackwrap_fn fn,
		    uint64_t fn_arg0, uint64_t fn_arg1, uint64_t fn_arg2)
    __attribute__ ((warn_unused_result));
void stackwrap_call_stack(void *stackpage, int freestack, stackwrap_fn fn,
			  uint64_t fn_arg0, uint64_t fn_arg1, uint64_t fn_arg2);

disk_io_status stackwrap_disk_io(disk_op op, struct part_desc *pd, void *buf,
				 uint32_t count, uint64_t offset)
    __attribute__ ((warn_unused_result));
disk_io_status stackwrap_disk_iov(disk_op op, struct part_desc *pd, 
				  struct kiovec *iov_buf, int iov_len, 
				  uint64_t offset)
    __attribute__ ((warn_unused_result));

struct lock {
    int locked;
    LIST_HEAD(lock_waiters_list, lock_waiter) waiters;
};

void lock_acquire(struct lock *l);
int  lock_try_acquire(struct lock *l);
void lock_release(struct lock *l);
void lock_init(struct lock *l);

struct stackwrap_state;
struct stackwrap_state *stackwrap_cur(void);
void stackwrap_wakeup(struct stackwrap_state *ss);
void stackwrap_sleep(struct stackwrap_state *ss);

#endif
