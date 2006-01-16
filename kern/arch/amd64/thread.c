#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/as.h>
#include <kern/segment.h>
#include <inc/elf64.h>
#include <inc/error.h>

struct Thread *cur_thread;
struct Thread_list thread_list_runnable;
struct Thread_list thread_list_limbo;

void
thread_set_runnable(struct Thread *t)
{
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(&thread_list_runnable, t, th_link);
    t->th_status = thread_runnable;
}

void
thread_suspend(struct Thread *t, struct Thread_list *waitq)
{
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(waitq, t, th_link);
    t->th_status = thread_suspended;
}

void
thread_halt(struct Thread *t)
{
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(&thread_list_limbo, t, th_link);
    t->th_status = thread_halted;
    if (cur_thread == t)
	cur_thread = 0;
}

int
thread_alloc(struct Label *l, struct Thread **tp)
{
    struct Thread *t;
    int r = kobject_alloc(kobj_thread, l, (struct kobject **)&t);
    if (r < 0)
	return r;

    t->th_status = thread_not_started;
    thread_swapin(t);

    *tp = t;
    return 0;
}

void
thread_swapin(struct Thread *t)
{
    t->th_as = 0;

    if (t->th_status == thread_suspended)
	t->th_status = thread_runnable;

    struct Thread_list *tq =
	(t->th_status == thread_runnable) ? &thread_list_runnable
					  : &thread_list_limbo;
    LIST_INSERT_HEAD(tq, t, th_link);

    // Threads are always pinned in-core
    kobject_incpin(&t->th_ko);
}

void
thread_swapout(struct Thread *t)
{
    kobject_decpin(&t->th_ko);
    LIST_REMOVE(t, th_link);

    if (t->th_as)
	kobject_decpin(&t->th_as->as_ko);
}

int
thread_gc(struct Thread *t)
{
    thread_halt(t);
    thread_swapout(t);
    return 0;
}

void
thread_run(struct Thread *t)
{
    if (t->th_status != thread_runnable)
	panic("trying to run a non-runnable thread %p", t);

    thread_switch(t);
    trapframe_pop(&t->th_tf);
}

void
thread_jump(struct Thread *t, struct Label *label,
	    struct cobj_ref as, void *entry,
	    void *stack, uint64_t arg0,
	    uint64_t arg1, uint64_t arg2)
{
    t->th_ko.ko_label = *label;

    if (t->th_as)
	kobject_decpin(&t->th_as->as_ko);
    t->th_as = 0;
    t->th_asref = as;

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uint64_t) entry;
    t->th_tf.tf_rsp = (uint64_t) stack;
    t->th_tf.tf_rdi = arg0;
    t->th_tf.tf_rsi = arg1;
    t->th_tf.tf_rdx = arg2;
}

void
thread_syscall_restart(struct Thread *t)
{
    t->th_tf.tf_rip -= 2;
}

void
thread_switch(struct Thread *t)
{
    cur_thread = t;
    as_switch(t->th_as);
}

int
thread_pagefault(struct Thread *t, void *fault_va)
{
    if (t->th_as == 0) {
	struct Address_space *as;
	int r = cobj_get(t->th_asref, kobj_address_space,
			 (struct kobject **)&as, iflow_read);
	if (r < 0)
	    return r;

	t->th_as = as;
	kobject_incpin(&t->th_as->as_ko);
    }

    return as_pagefault(t->th_as, fault_va);
}
