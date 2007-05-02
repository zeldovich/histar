#include <machine/lnxthread.h>
#include <machine/lnxopts.h>
#include <machine/lnxpage.h>
#include <machine/utrap.h>
#include <kern/arch.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <inc/queue.h>
#include <inc/error.h>

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

    e->cb(e->arg, t);
}

void
lnx64_schedule_loop(void)
{
    in_schedule_loop = 1;

    if (lnx64_stack_gc && sigsetjmp(env, 1) != 0)
	lnx64_thread_cb(arch_run_t);

    thread_run();
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

void
thread_arch_idle(void)
{
    printf("system idle, exiting\n");
    exit(0);
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    return -E_INVAL;
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT_NMASK | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uintptr_t) te->te_entry;
    t->th_tf.tf_rsp = (uintptr_t) te->te_stack;
    t->th_tf.tf_rdi = te->te_arg[0];
    t->th_tf.tf_rsi = te->te_arg[1];
    t->th_tf.tf_rdx = te->te_arg[2];
    t->th_tf.tf_rcx = te->te_arg[3];
    t->th_tf.tf_r8  = te->te_arg[4];
    t->th_tf.tf_r9  = te->te_arg[5];

    static_assert(thread_entry_narg == 6);
}

int
thread_arch_utrap(struct Thread *t, int selftrap,
		  uint32_t src, uint32_t num, uint64_t arg)
{
    void *stacktop;
    uint64_t rsp = t->th_tf.tf_rsp;
    if (rsp > t->th_as->as_utrap_stack_base &&
	rsp <= t->th_as->as_utrap_stack_top)
    {
	// Skip red zone (see ABI spec)
	stacktop = (void *) (uintptr_t) rsp - 128;
    } else {
	stacktop = (void *) t->th_as->as_utrap_stack_top;
    }

    struct UTrapframe t_utf;
    t_utf.utf_trap_src = src;
    t_utf.utf_trap_num = num;
    t_utf.utf_trap_arg = arg;
#define UTF_COPY(r) t_utf.utf_##r = t->th_tf.tf_##r
    UTF_COPY(rax);  UTF_COPY(rbx);  UTF_COPY(rcx);  UTF_COPY(rdx);
    UTF_COPY(rsi);  UTF_COPY(rdi);  UTF_COPY(rbp);  UTF_COPY(rsp);
    UTF_COPY(r8);   UTF_COPY(r9);   UTF_COPY(r10);  UTF_COPY(r11);
    UTF_COPY(r12);  UTF_COPY(r13);  UTF_COPY(r14);  UTF_COPY(r15);
    UTF_COPY(rip);  UTF_COPY(rflags);
#undef UTF_COPY

    // If sending a utrap to self, pretend that the system call completed
    if (selftrap)
	t_utf.utf_rip += 2;

    struct UTrapframe *utf = stacktop - sizeof(*utf);
    int r = check_user_access(utf, sizeof(*utf), SEGMAP_WRITE);
    if (r < 0) {
	if ((uintptr_t) utf <= t->th_as->as_utrap_stack_base)
	    cprintf("thread_arch_utrap: utrap stack overflow\n");
	return r;
    }

    memcpy(utf, &t_utf, sizeof(*utf));
    t->th_tf.tf_rsp = (uintptr_t) utf;
    t->th_tf.tf_rip = t->th_as->as_utrap_entry;
    t->th_tf.tf_rflags &= ~FL_TF;
    t->th_tf.tf_cs = GD_UT_MASK;
    return 0;
}
