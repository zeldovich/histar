#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <kern/segment.h>
#include <inc/elf64.h>
#include <inc/error.h>

struct Thread *cur_thread;
struct Thread_list thread_list;

void
thread_set_runnable(struct Thread *t)
{
    t->th_status = thread_runnable;
}

void
thread_suspend(struct Thread *t)
{
    t->th_status = thread_suspended;
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
    t->th_pgmap = &bootpml4;
    LIST_INSERT_HEAD(&thread_list, t, th_link);

    if (t->th_status == thread_suspended)
	t->th_status = thread_runnable;
}

void
thread_swapout(struct Thread *t)
{
    LIST_REMOVE(t, th_link);
    if (t->th_pgmap && t->th_pgmap != &bootpml4)
	page_map_free(t->th_pgmap);
}

void
thread_gc(struct Thread *t)
{
    thread_halt(t);
    thread_swapout(t);
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
thread_halt(struct Thread *t)
{
    t->th_status = thread_halted;
    if (cur_thread == t)
	cur_thread = 0;
}

void
thread_jump(struct Thread *t, struct Label *label,
	    struct segment_map *segmap, void *entry,
	    void *stack, uint64_t arg)
{
    t->th_ko.ko_label = *label;

    if (t->th_pgmap && t->th_pgmap != &bootpml4)
	page_map_free(t->th_pgmap);
    t->th_pgmap = &bootpml4;

    t->th_segmap = *segmap;

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uint64_t) entry;
    t->th_tf.tf_rsp = (uint64_t) stack;
    t->th_tf.tf_rdi = arg;
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
    lcr3(kva2pa(t->th_pgmap));
}

int
thread_pagefault(void *fault_va)
{
    if (cur_thread->th_pgmap == &bootpml4) {
	int r = page_map_alloc(&cur_thread->th_pgmap);
	if (r < 0)
	    return r;
    }

    int r = segment_map_fill_pmap(&cur_thread->th_segmap,
				  cur_thread->th_pgmap, fault_va);
    if (r == -E_RESTART)
	return 0;

    if (r < 0) {
	cprintf("thread_pagefault: cannot fill pagemap: %d\n", r);
	return r;
    }

    return 0;
}
