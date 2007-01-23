#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <inc/queue.h>
#include <machine/lnxthread.h>
#include <machine/lnxopts.h>
#include <machine/lnxpage.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>

static int in_schedule_loop;
static sigjmp_buf env;
static struct Thread *arch_run_t;

struct thread_cb_ent {
    kobject_id_t tid;
    lnx64_thread_cb_t cb;
    void *arg;
    LIST_ENTRY(thread_cb_ent) link;
};
static LIST_HEAD(, thread_cb_ent) cb_head;

static struct thread_cb_ent *
lnx64_find_thread_cb(kobject_id_t tid)
{
    struct thread_cb_ent *e;
    LIST_FOREACH(e, &cb_head, link)
	if (e->tid == tid)
	    return e;
    return 0;
}

void
lnx64_set_thread_cb(kobject_id_t tid, lnx64_thread_cb_t cb, void *arg)
{
    struct thread_cb_ent *e = lnx64_find_thread_cb(tid);
    if (cb) {
	if (!e) {
	    e = malloc(sizeof(*e));
	    e->tid = tid;
	    LIST_INSERT_HEAD(&cb_head, e, link);
	}
	e->cb = cb;
	e->arg = arg;
    } else {
	if (e) {
	    LIST_REMOVE(e, link);
	    free(e);
	}
    }
}

static void
lnx64_thread_cb(struct Thread *t)
{
    struct thread_cb_ent *e = lnx64_find_thread_cb(t->th_ko.ko_id);
    if (!e) {
	printf("lnx64_thread_cb: missing callback for thread %"PRIu64,
	       t->th_ko.ko_id);
	exit(-1);
    }

    if (lnx64_pmap_prefill)
	lnxpmap_prefill();

    e->cb(e->arg, t);
}

void
lnx64_schedule_loop(void)
{
    in_schedule_loop = 1;

    if (lnx64_stack_gc && sigsetjmp(env, 1) != 0)
	lnx64_thread_cb(arch_run_t);

    if (!cur_thread)
	schedule();
    thread_run(cur_thread);
    printf("lnx64_schedule: thread_run returned!\n");
    exit(-1);
}

void
thread_arch_run(const struct Thread *ct)
{
    if (!in_schedule_loop) {
	printf("thread_arch_run: you forgot to call lnx64_schedule_loop\n");
	exit(-1);
    }

    struct Thread *t = &kobject_dirty(&ct->th_ko)->th;

    static uint64_t sched_tsc;
    sched_start(t, sched_tsc);
    sched_tsc++;
    sched_stop(t, sched_tsc);

    if (lnx64_stack_gc) {
	arch_run_t = t;
	siglongjmp(env, 1);
    } else {
	lnx64_thread_cb(t);
	lnx64_schedule_loop();
    }
}
