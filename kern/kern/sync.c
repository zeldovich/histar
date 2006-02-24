#include <machine/thread.h>
#include <kern/sync.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <inc/error.h>

static struct Thread_list sync_waiting;
static int sync_debug = 0;

int
sync_wait(uint64_t *addr, uint64_t val, uint64_t wakeup_msec)
{
    if (sync_debug)
	cprintf("sync_wait: addr %p val %lx wakeup %lx now %lx\n",
		addr, val, wakeup_msec, timer_user_msec);

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

void
sync_wakeup_addr(uint64_t *addr)
{
    if (sync_debug)
	cprintf("sync_wakeup_addr: addr %p\n", addr);

    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_addr == PGOFF(addr))
	    thread_set_runnable(t);

	t = next;
    }
}

void
sync_wakeup_timer(void)
{
    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_msec <= timer_user_msec) {
	    if (sync_debug)
		cprintf("sync_wakeup_timer: waking up %lx now %lx\n",
			t->th_wakeup_msec, timer_user_msec);

	    thread_set_runnable(t);
	}

	t = next;
    }
}
