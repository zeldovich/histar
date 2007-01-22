#include <machine/lnxinit.h>
#include <machine/actor.h>
#include <machine/lnxthread.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>
#include <inc/arc4.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void
tcb(void *arg, struct Thread *t)
{
    char *name = (char *) arg;
    printf("tcb[%s]: tid %"PRIu64", t->rip = %"PRIx64"\n",
	   name, t->th_ko.ko_id, t->th_tf.tf_rip);

    schedule();
}

int
main(int argc, char **av)
{
    const char *disk_pn = "/dev/null";
    const char *cmdline = "pstate=discard";

    lnx64_init(disk_pn, cmdline, 8 * 1024 * 1024);
    printf("HiStar/lnx64..\n");

    actor_init();

    struct actor a;
    actor_create(&a, 0);
    lnx64_set_thread_cb(a.thread_id, &tcb, "a");

    struct actor b;
    actor_create(&b, 0);
    lnx64_set_thread_cb(b.thread_id, &tcb, "b");

    lnx64_schedule_loop();
}
