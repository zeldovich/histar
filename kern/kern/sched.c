#include <kern/sched.h>
#include <kern/lib.h>
#include <machine/thread.h>

// A really slow round-robin scheduler

static struct Thread *last_thread;

static int
is_last_alive()
{
    struct Thread *t;
    LIST_FOREACH(t, &thread_list, link)
	if (t == last_thread)
	    return 1;
    return 0;
}

void
schedule()
{
    struct Thread *next = 0;

    if (is_last_alive())
	next = LIST_NEXT(last_thread, link);
    if (next == 0)
	next = LIST_FIRST(&thread_list);

    if (next == 0)
	panic("no runnable threads");

    last_thread = next;
    thread_run(next);
}
