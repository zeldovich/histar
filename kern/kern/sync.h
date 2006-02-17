#ifndef JOS_KERN_SYNC_H
#define JOS_KERN_SYNC_H

#include <machine/types.h>

int  sync_wait(uint64_t *addr, uint64_t val);
int  sync_wakeup(uint64_t *addr);

#endif
