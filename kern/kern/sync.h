#ifndef JOS_KERN_SYNC_H
#define JOS_KERN_SYNC_H

#include <machine/types.h>

int  sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_at_msec);
void sync_wakeup_addr(uint64_t *addr);
void sync_wakeup_timer(void);

#endif
