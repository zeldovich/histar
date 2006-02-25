#ifndef JOS_KERN_TIMER_H
#define JOS_KERN_TIMER_H

#include <machine/types.h>
#include <machine/thread.h>
#include <dev/kclock.h>
#include <inc/queue.h>

extern uint64_t timer_user_msec;
extern uint64_t timer_user_msec_offset;

struct periodic_task {
    // external
    void (*pt_fn) (void);
    uint64_t pt_interval_ticks;

    // internal
    uint64_t pt_wakeup_ticks;
    LIST_ENTRY(periodic_task) pt_link;
};

void timer_init(void);
void timer_add_periodic(struct periodic_task *pt);

#endif
