#include <machine/lnxinit.h>
#include <machine/lnxthread.h>
#include <kern/syscall.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>
#include <kern/lib.h>
#include <inc/arc4.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static uint64_t root_container_id;

static void
bootstrap_tcb(void *arg, struct Thread *t)
{
    printf("tcb[%s]: tid %"PRIu64", t->rip = %"PRIx64"\n",
	   t->th_ko.ko_name, t->th_ko.ko_id, t->th_tf.tf_rip);

    if (++t->th_tf.tf_rip > 10)
	kern_syscall(SYS_self_halt, 0, 0, 0, 0, 0, 0, 0);
    schedule();
}


static void
bootstrap_stuff(void)
{
    struct Label *rcl;
    assert(0 == label_alloc(&rcl, 1));

    struct Container *rc;
    assert(0 == container_alloc(rcl, &rc));
    rc->ct_ko.ko_quota_total = CT_QUOTA_INF;
    kobject_incref_resv(&rc->ct_ko, 0);
    root_container_id = rc->ct_ko.ko_id;

    struct Label *tc;
    assert(0 == label_alloc(&tc, 2));

    struct Thread *t;
    assert(0 == thread_alloc(rcl, tc, &t));
    assert(0 == container_put(rc, &t->th_ko));
    thread_set_runnable(t);
    thread_set_sched_parents(t, rc->ct_ko.ko_id, 0);
    sprintf(t->th_ko.ko_name, "bootstrap");
    lnx64_set_thread_cb(t->th_ko.ko_id, &bootstrap_tcb, 0);
}

int
main(int argc, char **av)
{
    const char *disk_pn = "/dev/null";
    const char *cmdline = "pstate=discard";

    lnx64_init(disk_pn, cmdline, 8 * 1024 * 1024);
    printf("HiStar/lnx64..\n");

    bootstrap_stuff();
    lnx64_schedule_loop();
}
