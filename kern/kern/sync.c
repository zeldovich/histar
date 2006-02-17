#include <machine/thread.h>
#include <kern/sync.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <inc/error.h>

static struct Thread_list sync_waiting;

int
sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_msec)
{
    if (*addr != val)
	return 0;
    if (wakeup_msec <= timer_user_msec)
	return 0;

    struct Thread *t = &kobject_dirty(&cur_thread->th_ko)->th;
    t->th_wakeup_msec = wakeup_msec;
    t->th_wakeup_addr = PGOFF(addr);

    thread_suspend(cur_thread, &sync_waiting);
    return -E_RESTART;
}

int
sync_wakeup_addr(uint64_t *addr)
{
    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_addr == PGOFF(addr))
	    thread_set_runnable(t);

	t = next;
    }

    return 0;
}

void
sync_wakeup_timer(void)
{
    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_msec <= timer_user_msec)
	    thread_set_runnable(t);

	t = next;
    }
}
