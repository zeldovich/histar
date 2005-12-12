#ifndef JOS_KERN_TIMER_H
#define JOS_KERN_TIMER_H

#include <machine/types.h>
#include <machine/thread.h>

extern uint64_t timer_ticks;
extern struct Thread_list timer_sleep;

void timer_intr(void);
void timer_init(void);

#endif
