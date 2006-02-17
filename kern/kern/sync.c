#include <machine/thread.h>
#include <kern/sync.h>
#include <inc/error.h>

static struct Thread_list sync_waiting;

int
sync_wait(uint64_t *addr, uint64_t val)
{
    if (*addr != val)
	return 0;

    thread_suspend(cur_thread, &sync_waiting);
    return -E_RESTART;
}

int
sync_wakeup(uint64_t *addr)
{
    while (!LIST_EMPTY(&sync_waiting)) {
	struct Thread *t = LIST_FIRST(&sync_waiting);
	thread_set_runnable(t);
    }

    return 0;
}
