#ifndef JOS_KERN_SYNC_H
#define JOS_KERN_SYNC_H

#include <machine/types.h>

int sync_wait(uint64_t **addrs, uint64_t *vals, uint64_t *refcts,
	      uint64_t num, uint64_t wakeup_msec)
    __attribute__ ((warn_unused_result));
int  sync_wakeup_addr(uint64_t *addr)
    __attribute__ ((warn_unused_result));
void sync_wakeup_timer(void);
void sync_wakeup_segment(struct cobj_ref seg);
void sync_remove_thread(const struct Thread *t);

#endif
