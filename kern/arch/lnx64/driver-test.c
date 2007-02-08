#include <machine/lnxinit.h>
#include <machine/lnxthread.h>
#include <machine/lnxpage.h>
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

#include <ft_public.h>
#include <ft_runtest.h>

#define make_symbolic(x, name) ft_make_symbolic_array(&(x), sizeof(x), (name))

static void
bootstrap_tcb(void *arg, struct Thread *t)
{
    unsigned char *upage = (unsigned char *) (uintptr_t) t->th_tf.tf_r15;

    uint64_t rip = t->th_tf.tf_rip;
    static uint64_t ncalls = 1;

    /*
     * Other operations we could do:
     *  memory reads/writes [no real page mapping, so only access valid things]
     *  trigger page faults: thread_pagefault(cur_thread, va, SEGMAP_WRITE|SEGMAP_EXEC);
     *  trap self: thread_utrap(cur_thread, UTRAP_SRC_HW, trapno, traparg);
     */

    if (rip == 0) {
	/* Set things up.. */
	ft_make_symbolic_array(upage, PGSIZE, "upage");
    } else if (rip <= ncalls) {
	uint64_t a0, a1, a2, a3, a4, a5, a6, a7;
	make_symbolic(a0, "syscall_a0");
	make_symbolic(a1, "syscall_a1");
	make_symbolic(a2, "syscall_a2");
	make_symbolic(a3, "syscall_a3");
	make_symbolic(a4, "syscall_a4");
	make_symbolic(a5, "syscall_a5");
	make_symbolic(a6, "syscall_a6");
	make_symbolic(a7, "syscall_a7");

	kern_syscall(a0, a1, a2, a3, a4, a5, a6, a7);
    } else if (rip == ncalls + 1) {
	kern_syscall(SYS_self_halt, 0, 0, 0, 0, 0, 0, 0);
    } else {
	printf("Strange rip value: %"PRIu64"\n", rip);
	assert(0);
    }

    t->th_tf.tf_rip++;
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

    // Figure out where to map this guy..
    void *upage = 0;
    assert(0 == kobject_get_page(&s->sg_ko, 0, &upage, page_shared_ro));
    assert(upage);

    struct Address_space *as;
    assert(0 == as_alloc(rcl, &as));
    assert(0 == container_put(rc, &as->as_ko));
    assert(0 == kobject_set_nbytes(&as->as_ko, PGSIZE));
    as->as_utrap_entry = 0xdeadbeef;
    as->as_utrap_stack_base = (uintptr_t) upage;
    as->as_utrap_stack_top = (uintptr_t) upage + PGSIZE;

    struct u_segment_mapping *usm;
    assert(0 == kobject_get_page(&as->as_ko, 0, (void **)&usm, page_excl_dirty));
    usm[0].segment = COBJ(rc->ct_ko.ko_id, s->sg_ko.ko_id);
    usm[0].start_page = 0;
    usm[0].num_pages = 1;
    usm[0].flags = SEGMAP_READ | SEGMAP_WRITE;
    usm[0].kslot = 0;
    usm[0].va = upage;

    struct Thread *t;
    assert(0 == thread_alloc(rcl, tc, &t));
    assert(0 == container_put(rc, &t->th_ko));
    sprintf(t->th_ko.ko_name, "bootstrap");
    t->th_asref = COBJ(rc->ct_ko.ko_id, as->as_ko.ko_id);
    thread_set_sched_parents(t, rc->ct_ko.ko_id, 0);
    thread_set_runnable(t);
    lnx64_set_thread_cb(t->th_ko.ko_id, &bootstrap_tcb, 0);

    t->th_tf.tf_rip = 0;
    t->th_tf.tf_r15 = (uintptr_t) upage;
}

int
main(int argc, char **av)
{
    const char *disk_pn = "/dev/null";
    const char *cmdline = "pstate=discard";

    lnx64_init(disk_pn, cmdline, 256 * 1024);
    printf("HiStar/lnx64..\n");

    bootstrap_stuff();

    /*
     * XXX the kernel has some unresolved issues with handling out-of-memory
     * conditions.  also, since we currently don't support disk IO with FT,
     * there's nothing we can do to handle an out-of-memory condition with
     * no disk to swap out to..
     */
    //enable_page_alloc_failure = 1;

    lnx64_schedule_loop();
}
