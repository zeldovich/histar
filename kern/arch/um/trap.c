#include <kern/arch.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>

static jmp_buf trap_jb;

void
thread_arch_run(const struct Thread *t)
{
    printf("pretending to have run thread %s (%"PRIu64")\n",
	    t->th_ko.ko_name, t->th_ko.ko_id);
    sched_stop(t, 100);

    static int setjmp_done;
    if (setjmp_done)
	longjmp(trap_jb, 1);

    setjmp_done = 1;
    setjmp(trap_jb);

    schedule();
    thread_run();
}

void
thread_arch_idle(void)
{
    printf("thread_arch_idle\n");
    exit(0);
}
