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
    char *buf = (char *) 0x90000000;
    sprintf(buf, "Hello world.\n");

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

    struct Segment *s;
    assert(0 == segment_alloc(rcl, &s));
    assert(0 == container_put(rc, &s->sg_ko));
    assert(0 == segment_set_nbytes(s, 4096));

    struct Address_space *as;
    assert(0 == as_alloc(rcl, &as));
    assert(0 == container_put(rc, &as->as_ko));
    assert(0 == kobject_set_nbytes(&as->as_ko, PGSIZE));

    struct u_segment_mapping *usm;
    assert(0 == kobject_get_page(&as->as_ko, 0, (void **)&usm, page_excl_dirty));
    usm[0].segment = COBJ(rc->ct_ko.ko_id, s->sg_ko.ko_id);
    usm[0].start_page = 0;
    usm[0].num_pages = 1;
    usm[0].flags = SEGMAP_READ | SEGMAP_WRITE;
    usm[0].kslot = 0;
    usm[0].va = (void *) 0x90000000;

    struct Thread *t;
    assert(0 == thread_alloc(rcl, tc, &t));
    assert(0 == container_put(rc, &t->th_ko));
    sprintf(t->th_ko.ko_name, "bootstrap");
    t->th_asref = COBJ(rc->ct_ko.ko_id, as->as_ko.ko_id);
    thread_set_sched_parents(t, rc->ct_ko.ko_id, 0);
    thread_set_runnable(t);
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
