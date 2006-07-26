#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/as.h>
#include <machine/utrap.h>
#include <kern/segment.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <inc/elf64.h>
#include <inc/error.h>

enum { thread_pf_debug = 0 };

const struct Thread *cur_thread;
struct Thread_list thread_list_runnable;
struct Thread_list thread_list_limbo;

static void
thread_pin(struct Thread *t)
{
    if (!t->th_pinned) {
	t->th_pinned = 1;
	kobject_pin_hdr(&t->th_ko);
    }
}

static void
thread_unpin(struct Thread *t)
{
    if (t->th_pinned) {
	t->th_pinned = 0;
	kobject_unpin_hdr(&t->th_ko);
    }
}

static void
thread_sched_adjust(struct Thread *t, int runnable)
{
    if (t->th_sched_joined && !runnable) {
	sched_leave(t);
	t->th_sched_joined = 0;
    }

    if (!t->th_sched_joined && runnable) {
	sched_join(t);
	t->th_sched_joined = 1;
    }
}

void
thread_set_runnable(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_sched_adjust(t, 0);
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(&thread_list_runnable, t, th_link);
    thread_sched_adjust(t, 1);

    t->th_status = thread_runnable;
    thread_pin(t);
}

void
thread_suspend(const struct Thread *const_t, struct Thread_list *waitq)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_sched_adjust(t, 0);
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(waitq, t, th_link);
    t->th_status = thread_suspended;
    thread_pin(t);
}

void
thread_halt(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_sched_adjust(t, 0);
    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(&thread_list_limbo, t, th_link);
    t->th_status = thread_halted;
    thread_unpin(t);
    if (cur_thread == t)
	cur_thread = 0;
}

int
thread_alloc(const struct Label *contaminate,
	     const struct Label *clearance,
	     struct Thread **tp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_thread, contaminate, &ko);
    if (r < 0)
	return r;

    struct Thread *t = &ko->th;
    t->th_sched_tickets = 1024;
    t->th_status = thread_not_started;
    t->th_ko.ko_flags |= KOBJ_LABEL_MUTABLE;
    r = kobject_set_label(&t->th_ko, kolabel_clearance, clearance);
    if (r < 0)
	return r;

    struct Segment *sg;
    r = segment_alloc(contaminate, &sg);
    if (r < 0)
	return r;

    r = segment_set_nbytes(sg, PGSIZE);
    if (r < 0)
	return r;

    r = kobject_incref(&sg->sg_ko, &t->th_ko);
    if (r < 0)
	return r;

    t->th_sg = sg->sg_ko.ko_id;
    thread_swapin(t);

    *tp = t;
    return 0;
}

void
thread_swapin(struct Thread *t)
{
    t->th_as = 0;
    t->th_pinned = 0;
    t->th_sched_joined = 0;

    if (SAFE_EQUAL(t->th_status, thread_suspended))
	t->th_status = thread_runnable;

    struct Thread_list *tq =
	SAFE_EQUAL(t->th_status, thread_runnable) ? &thread_list_runnable
						  : &thread_list_limbo;
    LIST_INSERT_HEAD(tq, t, th_link);

    // Runnable and suspended threads are pinned
    if (SAFE_EQUAL(t->th_status, thread_runnable)) {
	thread_sched_adjust(t, 1);
	thread_pin(t);
    }
}

static void
thread_clear_as(struct Thread *t)
{
    if (t->th_as) {
	kobject_unpin_hdr(&t->th_as->as_ko);
	t->th_as = 0;
    }
}

void
thread_swapout(struct Thread *t)
{
    thread_unpin(t);
    thread_sched_adjust(t, 0);
    LIST_REMOVE(t, th_link);

    thread_clear_as(t);
}

void
thread_zero_refs(const struct Thread *t)
{
    thread_halt(t);
}

int
thread_gc(struct Thread *t)
{
    if (t->th_sg) {
	const struct kobject *ko;
	int r = kobject_get(t->th_sg, &ko, kobj_segment, iflow_none);
	if (r < 0)
	    return r;

	kobject_decref(&ko->hdr, &t->th_ko);
	t->th_sg = 0;
    }

    thread_swapout(t);
    return 0;
}

void
thread_run(const struct Thread *t)
{
    if (!SAFE_EQUAL(t->th_status, thread_runnable))
	panic("trying to run a non-runnable thread %p", t);

    thread_switch(t);
    trap_user_iret_tsc = read_tsc();

    if (t->th_fp_enabled) {
	void *p;
	assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_shared_ro));
	lcr0(rcr0() & ~CR0_TS);
	fxrstor((const struct Fpregs *) p);
    } else {
	lcr0(rcr0() | CR0_TS);
    }

    sched_start(t);
    trapframe_pop(&t->th_tf);
}

int
thread_enable_fp(const struct Thread *const_t)
{
    int r;
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    if (t->th_fp_enabled)
	return 0;

    if (!t->th_fp_space) {
	r = kobject_set_nbytes(&t->th_ko, sizeof(struct Fpregs));
	if (r < 0)
	    return r;

	t->th_fp_space = 1;
    }

    struct Fpregs *fpreg;
    r = kobject_get_page(&t->th_ko, 0, (void **) &fpreg, page_excl_dirty);
    if (r < 0)
	return r;

    // Linux says so.
    memset(fpreg, 0, sizeof(*fpreg));
    fpreg->cwd = 0x37f;
    fpreg->mxcsr = 0x1f80;

    t->th_fp_enabled = 1;
    return 0;
}

void
thread_disable_fp(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    t->th_fp_enabled = 0;
}

int
thread_change_label(const struct Thread *const_t,
		    const struct Label *new_label)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    const struct kobject *ko_sg;
    int r = kobject_get(t->th_sg, &ko_sg, kobj_segment, iflow_rw);
    if (r < 0)
	return r;

    // Prepare labels for all of the objects
    const struct Label *cur_th_label, *cur_sg_label;
    r = kobject_get_label(&t->th_ko, kolabel_contaminate, &cur_th_label);
    if (r < 0)
	return r;

    r = kobject_get_label(&ko_sg->hdr, kolabel_contaminate, &cur_sg_label);
    if (r < 0)
	return r;

    // Copy with current label first, because kobject_alloc() checks
    // that you can write to the newly allocated object.
    struct Segment *sg_new;
    r = segment_copy(&ko_sg->sg, cur_th_label, &sg_new);
    if (r < 0)
	return r;

    // Pin the size of the segment at PGSIZE
    r = segment_set_nbytes(sg_new, PGSIZE);
    if (r < 0)
	return r;

    // Reserve some quota..
    struct kobject_quota_resv qr_th, qr_sg;
    kobject_qres_init(&qr_th, &t->th_ko);
    kobject_qres_init(&qr_sg, &sg_new->sg_ko);

    r = kobject_qres_reserve(&qr_sg, &new_label->lb_ko);
    if (r < 0)
	goto qrelease;

    r = kobject_qres_reserve(&qr_th, &sg_new->sg_ko);
    if (r < 0)
	goto qrelease;

    r = kobject_qres_reserve(&qr_th, &new_label->lb_ko);
    if (r < 0)
	goto qrelease;

    // Commit point
    t->th_sg = sg_new->sg_ko.ko_id;
    kobject_decref(&ko_sg->hdr,	&t->th_ko);
    kobject_incref_resv(&sg_new->sg_ko, &qr_th);

    kobject_set_label_prepared(&t->th_ko, kolabel_contaminate,
			       cur_th_label, new_label, &qr_th);
    kobject_set_label_prepared(&sg_new->sg_ko, kolabel_contaminate,
			       cur_sg_label, new_label, &qr_sg);

    // make sure all label checks get re-evaluated
    if (t->th_as)
	as_invalidate_label(t->th_as, 1);

    return 0;

qrelease:
    kobject_qres_release(&qr_th);
    kobject_qres_release(&qr_sg);
    return r;
}

void
thread_change_as(const struct Thread *const_t, struct cobj_ref as)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_clear_as(t);
    t->th_asref = as;
}

int
thread_jump(const struct Thread *const_t,
	    const struct Label *label,
	    const struct Label *clearance,
	    const struct thread_entry *te)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    const struct Label *cur_clearance;
    int r = kobject_get_label(&t->th_ko, kolabel_clearance, &cur_clearance);
    if (r < 0)
	return r;

    struct kobject_quota_resv qr_th;
    kobject_qres_init(&qr_th, &t->th_ko);

    r = kobject_qres_reserve(&qr_th, &clearance->lb_ko);
    if (r < 0)
	return r;

    r = thread_change_label(t, label);
    if (r < 0) {
	kobject_qres_release(&qr_th);
	return r;
    }

    kobject_set_label_prepared(&t->th_ko, kolabel_clearance,
			       cur_clearance, clearance, &qr_th);
    thread_change_as(t, te->te_as);

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uint64_t) te->te_entry;
    t->th_tf.tf_rsp = (uint64_t) te->te_stack;
    t->th_tf.tf_rdi = te->te_arg[0];
    t->th_tf.tf_rsi = te->te_arg[1];
    t->th_tf.tf_rdx = te->te_arg[2];
    t->th_tf.tf_rcx = te->te_arg[3];
    t->th_tf.tf_r8  = te->te_arg[4];
    t->th_tf.tf_r9  = te->te_arg[5];

    return 0;
}

void
thread_syscall_restart(const struct Thread *t)
{
    kobject_dirty(&t->th_ko)->th.th_tf.tf_rip -= 2;
}

void
thread_switch(const struct Thread *t)
{
    cur_thread = t;
    as_switch(t->th_as);
}

static int
thread_load_as(const struct Thread *t)
{
    if (t->th_as)
	return 0;

    const struct kobject *ko;
    int r = cobj_get(t->th_asref, kobj_address_space, &ko, iflow_read);
    if (r < 0)
	return r;

    const struct Address_space *as = &ko->as;
    kobject_ephemeral_dirty(&t->th_ko)->th.th_as = as;
    kobject_pin_hdr(&t->th_as->as_ko);

    as_invalidate_label(as, 0);
    return 0;
}

int
thread_pagefault(const struct Thread *t, void *fault_va, uint32_t reqflags)
{
    int r = thread_load_as(t);
    if (r < 0)
	return r;

    r = as_pagefault(t->th_as, fault_va, reqflags);
    if (r >= 0 || r == -E_RESTART)
	return r;

    if (thread_pf_debug)
	cprintf("thread_pagefault(th %ld %s, as %ld %s, va %p): %s\n",
		t->th_ko.ko_id, &t->th_ko.ko_name[0],
		t->th_as->as_ko.ko_id, &t->th_as->as_ko.ko_name[0],
		fault_va, e2s(r));

    r = thread_utrap(t, UTRAP_SRC_HW, T_PGFLT, (uint64_t) fault_va);
    if (r >= 0 || r == -E_RESTART)
	return r;

    cprintf("thread_pagefault: utrap: %s\n", e2s(r));
    return r;
}

int
thread_utrap(const struct Thread *const_t, uint32_t src, uint32_t num, uint64_t arg)
{
    if (!SAFE_EQUAL(const_t->th_status, thread_runnable) &&
	!SAFE_EQUAL(const_t->th_status, thread_suspended))
	return -E_INVAL;

    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    int r = thread_load_as(t);
    if (r < 0)
	return r;

    // Switch the current thread to the trap target thread, temporarily,
    // to ensure its privileges & thread-local segment are used.
    const struct Thread *saved_cur = cur_thread;
    cur_thread = t;

    // Switch to trap target thread's address space.
    as_switch(t->th_as);

    void *stacktop;
    uint64_t rsp = t->th_tf.tf_rsp;
    if (rsp > t->th_as->as_utrap_stack_base &&
	rsp <= t->th_as->as_utrap_stack_top)
	stacktop = (void *) rsp - 128;	// Skip red zone (see ABI spec)
    else
	stacktop = (void *) t->th_as->as_utrap_stack_top;

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

    struct UTrapframe *utf = stacktop - sizeof(*utf);
    r = check_user_access(utf, sizeof(*utf), SEGMAP_WRITE);
    if (r < 0)
	goto out;

    memcpy(utf, &t_utf, sizeof(*utf));
    t->th_tf.tf_rsp = (uint64_t) utf;
    t->th_tf.tf_rip = t->th_as->as_utrap_entry;
    t->th_tf.tf_rflags &= ~FL_TF;
    thread_set_runnable(t);

out:
    as_switch(cur_thread->th_as);
    cur_thread = saved_cur;
    return r;
}
