#include <machine/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>

// A really slow round-robin scheduler

static struct Thread *last_thread;

static int
is_last_alive(void)
{
    struct Thread *t;
    LIST_FOREACH(t, &thread_list_runnable, th_link)
	if (t == last_thread)
	    return 1;
    return 0;
}

void
schedule(void)
{
    struct Thread *next = 0;

    if (is_last_alive())
	next = LIST_NEXT(last_thread, th_link);
    if (next == 0)
	next = LIST_FIRST(&thread_list_runnable);
    if (next == 0)
	panic("no runnable threads");

    last_thread = next;
    thread_run(next);
}
