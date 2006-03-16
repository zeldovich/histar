#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/as.h>
#include <machine/utrap.h>
#include <kern/segment.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <inc/elf64.h>
#include <inc/error.h>

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

void
thread_set_runnable(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(&thread_list_runnable, t, th_link);
    t->th_status = thread_runnable;
    thread_pin(t);
}

void
thread_suspend(const struct Thread *const_t, struct Thread_list *waitq)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    LIST_REMOVE(t, th_link);
    LIST_INSERT_HEAD(waitq, t, th_link);
    t->th_status = thread_suspended;
    thread_pin(t);
}

void
thread_halt(const struct Thread *const_t)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

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
    t->th_status = thread_not_started;
    t->th_ko.ko_flags |= KOBJ_LABEL_MUTABLE;
    kobject_set_label_prepared(&t->th_ko, kolabel_clearance, 0, clearance);

    struct Segment *sg;
    r = segment_alloc(contaminate, &sg);
    if (r < 0)
	return r;

    t->th_sg = sg->sg_ko.ko_id;
    kobject_incref(&sg->sg_ko);

    r = segment_set_nbytes(sg, PGSIZE);
    if (r < 0)
	return r;
    sg->sg_ko.ko_min_bytes = PGSIZE;

    struct Container *ct;
    r = container_alloc(contaminate, &ct);
    if (r < 0)
	return r;

    t->th_ct = ct->ct_ko.ko_id;
    kobject_incref(&ct->ct_ko);
    ct->ct_ko.ko_flags |= KOBJ_LABEL_MUTABLE;
    ct->ct_avoid[kobj_container] = 1;
    ct->ct_avoid[kobj_thread] = 1;
    ct->ct_avoid[kobj_mlt] = 1;

    r = container_put(ct, &sg->sg_ko);
    if (r < 0)
	return r;

    thread_swapin(t);

    *tp = t;
    return 0;
}

void
thread_swapin(struct Thread *t)
{
    t->th_as = 0;
    t->th_pinned = 0;

    if (SAFE_EQUAL(t->th_status, thread_suspended))
	t->th_status = thread_runnable;

    struct Thread_list *tq =
	SAFE_EQUAL(t->th_status, thread_runnable) ? &thread_list_runnable
						  : &thread_list_limbo;
    LIST_INSERT_HEAD(tq, t, th_link);

    // Runnable and suspended threads are pinned
    if (SAFE_EQUAL(t->th_status, thread_runnable))
	thread_pin(t);
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
    const struct kobject *ko;
    int r;

    if (t->th_sg) {
	r = kobject_get(t->th_sg, &ko, kobj_segment, iflow_none);
	if (r < 0)
	    return r;

	kobject_decref(&ko->hdr);
	t->th_sg = 0;
    }

    if (t->th_ct) {
	r = kobject_get(t->th_ct, &ko, kobj_container, iflow_none);
	if (r < 0)
	    return r;

	kobject_decref(&ko->hdr);
	t->th_ct = 0;
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
    trapframe_pop(&t->th_tf);
}

int
thread_change_label(const struct Thread *const_t,
		    const struct Label *new_label)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    const struct kobject *ko_sg, *ko_ct;
    int r = kobject_get(t->th_sg, &ko_sg, kobj_segment, iflow_rw);
    if (r < 0)
	return r;

    r = kobject_get(t->th_ct, &ko_ct, kobj_container, iflow_rw);
    if (r < 0)
	return r;

    // Prepare labels for all of the objects
    const struct Label *cur_th_label, *cur_sg_label, *cur_ct_label;
    r = kobject_get_label(&t->th_ko, kolabel_contaminate, &cur_th_label);
    if (r < 0)
	return r;

    r = kobject_get_label(&ko_sg->hdr, kolabel_contaminate, &cur_sg_label);
    if (r < 0)
	return r;

    r = kobject_get_label(&ko_ct->hdr, kolabel_contaminate, &cur_ct_label);
    if (r < 0)
	return r;

    // Copy with current label first, because kobject_alloc() checks
    // that you can write to the newly allocated object.
    struct Segment *sg_new;
    r = segment_copy(&ko_sg->sg, cur_th_label, &sg_new);
    if (r < 0)
	return r;

    // Remove the old segment from the thread container, add the new one
    struct Container *ct = &kobject_dirty(&ko_ct->hdr)->ct;
    r = container_unref(ct, &ko_sg->hdr);
    if (r < 0)
	return r;

    r = container_put(ct, &sg_new->sg_ko);
    if (r < 0)
	return r;

    // Commit point
    t->th_sg = sg_new->sg_ko.ko_id;
    kobject_decref(&ko_sg->hdr);
    kobject_incref(&sg_new->sg_ko);

    kobject_set_label_prepared(&t->th_ko,      kolabel_contaminate, cur_th_label, new_label);
    kobject_set_label_prepared(&sg_new->sg_ko, kolabel_contaminate, cur_sg_label, new_label);
    kobject_set_label_prepared(&ct->ct_ko,     kolabel_contaminate, cur_ct_label, new_label);

    // make sure all label checks get re-evaluated
    thread_clear_as(t);

    return 0;
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
	    struct cobj_ref as, void *entry,
	    void *stack, uint64_t arg0,
	    uint64_t arg1)
{
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;

    const struct Label *cur_clearance;
    int r = kobject_get_label(&t->th_ko, kolabel_clearance, &cur_clearance);
    if (r < 0)
	return r;

    r = thread_change_label(t, label);
    if (r < 0)
	return r;

    kobject_set_label_prepared(&t->th_ko, kolabel_clearance,
			       cur_clearance, clearance);
    thread_change_as(t, as);

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uint64_t) entry;
    t->th_tf.tf_rsp = (uint64_t) stack;
    t->th_tf.tf_rdi = arg0;
    t->th_tf.tf_rsi = arg1;

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
    kobject_dirty(&t->th_ko)->th.th_as = as;
    kobject_pin_hdr(&t->th_as->as_ko);

    // Just to ensure all label checks are up-to-date.
    as_invalidate(as);
    return 0;
}

int
thread_pagefault(const struct Thread *t, void *fault_va, uint32_t reqflags)
{
    int r = thread_load_as(t);
    if (r < 0)
	return r;

    r = as_pagefault(&kobject_dirty(&t->th_as->as_ko)->as, fault_va, reqflags);
    if (r >= 0 || r == -E_RESTART)
	return r;

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
    struct Thread *t = &kobject_dirty(&const_t->th_ko)->th;
    int r = thread_load_as(t);
    if (r < 0)
	return r;

    as_switch(t->th_as);

    void *stacktop;
    uint64_t rsp = t->th_tf.tf_rsp;
    if (rsp > UTRAPSTACK && rsp < UTRAPSTACKTOP)
	stacktop = (void *) rsp - 8;
    else
	stacktop = (void *) UTRAPSTACKTOP;

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
    t->th_tf.tf_rip = UTRAPHANDLER;
    thread_set_runnable(t);

out:
    as_switch(cur_thread->th_as);
    return r;
}
