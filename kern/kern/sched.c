#include <machine/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>

void
schedule(void)
{
    if (cur_thread)
	cur_thread = LIST_NEXT(cur_thread, th_link);
    if (cur_thread == 0 || cur_thread->th_status != thread_runnable)
	cur_thread = LIST_FIRST(&thread_list_runnable);
    if (cur_thread == 0)
	panic("no runnable threads");
}
