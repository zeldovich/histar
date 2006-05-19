#include <machine/thread.h>
#include <kern/sync.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <inc/error.h>

static struct Thread_list sync_waiting;
static int sync_debug = 0;

static int
sync_invert(uint64_t *addr, uint64_t *seg_id, uint64_t *offset)
{
    *seg_id = 0;
    *offset = PGOFF(addr);
    return 0;
}

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

    struct Thread *t = &kobject_ephemeral_dirty(&cur_thread->th_ko)->th;
    t->th_wakeup_msec = wakeup_msec;

    int r = sync_invert(addr, &t->th_wakeup_seg_id, &t->th_wakeup_offset);
    if (r < 0)
	return r;

    thread_suspend(cur_thread, &sync_waiting);
    return 0;
}

int
sync_wakeup_addr(uint64_t *addr)
{
    if (sync_debug)
	cprintf("sync_wakeup_addr: addr %p\n", addr);

    uint64_t seg_id, offset;
    int r = sync_invert(addr, &seg_id, &offset);
    if (r < 0)
	return r;

    struct Thread *t = LIST_FIRST(&sync_waiting);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_seg_id == seg_id && t->th_wakeup_offset == offset)
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

	if (t->th_wakeup_msec <= timer_user_msec) {
	    if (sync_debug)
		cprintf("sync_wakeup_timer: waking up %lx now %lx\n",
			t->th_wakeup_msec, timer_user_msec);

	    thread_set_runnable(t);
	}

	t = next;
    }
}
