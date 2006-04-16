#ifndef JOS_KERN_STACKWRAP_H
#define JOS_KERN_STACKWRAP_H

#include <machine/types.h>
#include <dev/disk.h>
#include <inc/queue.h>

typedef void (*stackwrap_fn) (void *, void *, void *);

int  stackwrap_call(stackwrap_fn fn,
		    void *fn_arg0, void *fn_arg1, void *fn_arg2)
    __attribute__ ((warn_unused_result));

disk_io_status stackwrap_disk_io(disk_op op, void *buf,
				 uint32_t count, uint64_t offset)
    __attribute__ ((warn_unused_result));
disk_io_status stackwrap_disk_iov(disk_op op, struct iovec *iov_buf,
				  int iov_len, uint64_t offset)
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
