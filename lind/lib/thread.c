#include <linux/signal.h>
#include <linux/kthread.h>

void 
linux_thread_run(int (*threadfn)(void *data), void *data,  const char *name)
{
    struct task_struct *p = kthread_run(threadfn, data, "poop");
    if (IS_ERR(p))
	panic("kthread_run error: %ld\n", PTR_ERR(p));
}

void 
linux_thread_flushsig(void)
{
    flush_signals(current);
}
