#ifndef JOS_KERN_TIMER_H
#define JOS_KERN_TIMER_H

#include <machine/types.h>
#include <dev/kclock.h>
#include <kern/thread.h>
#include <inc/queue.h>

struct time_source {
    uint64_t freq_hz;
    void *arg;
    uint64_t (*ticks) (void *);
    void (*delay_nsec) (void *, uint64_t);
};

struct preemption_timer {
    void *arg;
    void (*schedule_nsec) (void *, uint64_t);
};

extern struct time_source *the_timesrc;
extern struct preemption_timer *the_schedtmr;

extern uint64_t timer_user_nsec_offset;		// used by pstate
uint64_t timer_user_nsec(void);
void timer_delay(uint64_t nsec);

/*
 * Periodic task handling.  schedule() calls timer_periodic_notify().
 */
struct periodic_task {
    // external
    void (*pt_fn) (void);
    uint64_t pt_interval_sec;

    // internal
    uint64_t pt_interval_ticks;
    uint64_t pt_last_ticks;
    LIST_ENTRY(periodic_task) pt_link;
};

void timer_periodic_notify(void);
void timer_add_periodic(struct periodic_task *pt);
void timer_remove_periodic(struct periodic_task *pt);

#endif
