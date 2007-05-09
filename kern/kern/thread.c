#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/trapcodes.h>
#include <machine/utrap.h>
#include <kern/segment.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/thread.h>
#include <kern/as.h>
#include <kern/sync.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pstate.h>
#include <inc/elf64.h>
#include <inc/error.h>
#include <inc/safeint.h>

enum { thread_pf_debug = 0 };

const struct Thread *cur_thread;
struct Thread_list *cur_waitlist;
struct Thread_list thread_list_runnable;

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
thread_unlink(struct Thread *t)
{
    if (t->th_linked) {
	LIST_REMOVE(t, th_link);
	t->th_linked = 0;
    }

    if (t->th_sync_waiting)
	sync_remove_thread(t);
}

static void
thread_link(struct Thread *t, struct Thread_list *tlist)
{
    assert(!t->th_linked);
    LIST_INSERT_HEAD(tlist, t, th_link);
    t->th_linked = 1;
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
    thread_unlink(t);
    thread_link(t, &thread_list_runnable);
    t->th_status = thread_runnable;
    thread_sched_adjust(t, 1);

    thread_pin(t);
}

void
thread_suspend(const struct Thread *const_t, struct Thread_list *waitq)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_sched_adjust(t, 0);
    thread_unlink(t);
    thread_link(t, waitq);
    t->th_status = thread_suspended;
    thread_pin(t);
}

void
thread_halt(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    thread_sched_adjust(t, 0);
    thread_unlink(t);
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
    t->th_sync_waiting = 0;
    t->th_linked = 0;

    /*
     * Zeroing out scheduler state means we can lose or gain resources
     * across swapout.  The reason is that th_sched_remain might be
     * negative, with an absolute value greater than global_pass.
     * So, on sched_join(), the thread's pass will underflow.
     */
    t->th_sched_remain = 0;

    if (SAFE_EQUAL(t->th_status, thread_suspended))
	t->th_status = thread_runnable;

    if (SAFE_EQUAL(t->th_status, thread_runnable)) {
	thread_link(t, &thread_list_runnable);

	// Runnable and suspended threads are pinned
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
    thread_unlink(t);

    thread_clear_as(t);
}

void
thread_on_decref(const struct Thread *t)
{
    if (t->th_ko.ko_ref == 0)
	thread_halt(t);
    else
	thread_check_sched_parents(t);
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
thread_run(void)
{
    pstate_swapout_check();

    if (!cur_thread || !SAFE_EQUAL(cur_thread->th_status, thread_runnable))
	schedule();

    if (!cur_thread)
	thread_arch_idle();

    if (!SAFE_EQUAL(cur_thread->th_status, thread_runnable))
	panic("trying to run a non-runnable thread %p", cur_thread);

    // Reload the AS, in case something changed
    thread_switch(cur_thread);
    thread_arch_run(cur_thread);
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
thread_set_waitslots(const struct Thread *const_t, uint64_t nslots)
{
    int overflow = 0;
    static_assert(sizeof(struct Fpregs) <= PGSIZE);
    uint64_t nbytes = safe_add64(&overflow, PGSIZE,
			         safe_mul64(&overflow,
					    sizeof(struct sync_wait_slot),
					    nslots));
    if (overflow)
	return -E_INVAL;

    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    int r = kobject_set_nbytes(&t->th_ko, nbytes); 
    if (r < 0)
	return r;

    t->th_multi_slots = nslots;
    t->th_fp_space = 1;
    return 0;
}

void
thread_set_sched_parents(const struct Thread *const_t, uint64_t p0, uint64_t p1)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    t->th_sched_parents[0] = p0;
    t->th_sched_parents[1] = p1;
    thread_check_sched_parents(t);
}

void
thread_check_sched_parents(const struct Thread *t)
{
    // Three reasons this does not affect non-current threads:
    // - scheduler will run this function before scheduling t
    // - need to use cur_thread's label for iflow checks
    // - don't want to put other threads to sleep for t's swapin
    if (!cur_thread || t->th_ko.ko_id != cur_thread->th_ko.ko_id)
	return;

    for (int i = 0; i < 2; i++) {
	const struct Container *c;
	int r = container_find(&c, t->th_sched_parents[i], iflow_read);
	if (r < 0) {
	    if (r == -E_RESTART)
		return;
	    continue;
	}

	r = container_has(c, t->th_ko.ko_id);
	if (r < 0) {
	    if (r == -E_RESTART)
		return;
	    continue;
	}

	// Success: no need to halt this thread.
	return;
    }

    cprintf("thread %"PRIu64" (%s) not self-aware (%"PRIu64" refs), halting\n",
	    t->th_ko.ko_id, t->th_ko.ko_name, t->th_ko.ko_ref);
    thread_halt(t);
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
    thread_check_sched_parents(t);

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

    t->th_cache_flush = 0;
    thread_arch_jump(t, te);

    return 0;
}

void
thread_switch(const struct Thread *t)
{
    cur_thread = t;
    as_switch(t->th_as);
}

int
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
	cprintf("thread_pagefault(th %"PRIu64" %s, as %"PRIu64
		" (%s), va %p): %s\n",
		t->th_ko.ko_id, &t->th_ko.ko_name[0],
		t->th_as->as_ko.ko_id, &t->th_as->as_ko.ko_name[0],
		fault_va, e2s(r));

    r = thread_utrap(t, UTRAP_SRC_HW, T_PGFLT, (uintptr_t) fault_va);
    if (r >= 0 || r == -E_RESTART)
	return r;

    cprintf("thread_pagefault: utrap: %s\n", e2s(r));
    return r;
}

int
thread_utrap(const struct Thread *const_t,
	     uint32_t src, uint32_t num, uint64_t arg)
{
    if (!SAFE_EQUAL(const_t->th_status, thread_runnable) &&
	!SAFE_EQUAL(const_t->th_status, thread_suspended))
	return -E_INVAL;

    if (const_t->th_tf.tf_cs == GD_UT_MASK)
	return -E_BUSY;

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

    r = thread_arch_utrap(t, src, num, arg);
    if (r >= 0)
	thread_set_runnable(t);

    as_switch(cur_thread->th_as);
    cur_thread = saved_cur;
    return r;
}

void
thread_suspend_cur(struct Thread_list *waitq)
{
    if (cur_thread) {
	thread_suspend(cur_thread, waitq);
    } else if (cur_waitlist) {
	const struct Thread *t = LIST_FIRST(cur_waitlist);
	thread_suspend(t, waitq);
    }
}
