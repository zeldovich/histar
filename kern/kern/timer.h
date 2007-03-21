#ifndef JOS_KERN_TIMER_H
#define JOS_KERN_TIMER_H

#include <machine/types.h>
#include <dev/kclock.h>
#include <kern/thread.h>
#include <inc/queue.h>

struct hw_timer {
    void *arg;
    uint64_t freq_hz;

    uint64_t (*ticks) (void *);
    void (*schedule) (void *, uint64_t);	// ticks
    void (*delay) (void *, uint64_t);		// nsec
};

extern struct hw_timer *the_timer;

extern uint64_t timer_user_nsec_offset;		// used by pstate
uint64_t timer_user_nsec(void);
void timer_delay(uint64_t nsec);

/*
 * Periodic task handling.
 */
struct periodic_task {
    // external
    void (*pt_fn) (void);
    uint64_t pt_interval_sec;

    // internal
    uint64_t pt_last_ticks;
    LIST_ENTRY(periodic_task) pt_link;
};

void timer_periodic_notify(void);
void timer_add_periodic(struct periodic_task *pt);
void timer_remove_periodic(struct periodic_task *pt);

#endif
