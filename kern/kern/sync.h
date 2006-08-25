#ifndef JOS_KERN_SYNC_H
#define JOS_KERN_SYNC_H

#include <machine/types.h>

int  sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_at_msec)
    __attribute__ ((warn_unused_result));
int sync_wait_multi(uint64_t **addrs, uint64_t *vals, uint64_t num, 
		    uint64_t wakeup_msec)
    __attribute__ ((warn_unused_result));
int  sync_wakeup_addr(uint64_t *addr)
    __attribute__ ((warn_unused_result));
void sync_wakeup_timer(void);

#endif
