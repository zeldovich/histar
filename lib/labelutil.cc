extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/jthread.hh>

static jthread_mutex_t label_ops_mu;
static uint64_t cur_th_label_id, cur_th_clear_id;
static label cur_th_label, cur_th_clear;

enum { handle_debug = 0 };

int
thread_set_label(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);

    int r = sys_self_set_label(l->to_ulabel());
    if (r < 0)
	return r;

    cur_th_label.copy_from(l);
    cur_th_label_id = thread_id();
    return 0;
}

int
thread_set_clearance(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);

    int r = sys_self_set_clearance(l->to_ulabel());
    if (r < 0)
	return r;

    cur_th_clear.copy_from(l);
    cur_th_clear_id = thread_id();
    return 0;
}

void
thread_drop_star(uint64_t handle)
{
    if (handle_debug)
	cprintf("[%ld] handle: dropping %ld\n", thread_id(), handle);

    try {
	label clear;
	thread_cur_clearance(&clear);
	if (clear.get(handle) != clear.get_default()) {
	    clear.set(handle, clear.get_default());
	    error_check(thread_set_clearance(&clear));
	}

	label self;
	thread_cur_label(&self);
	if (self.get(handle) != self.get_default()) {
	    self.set(handle, self.get_default());
	    error_check(thread_set_label(&self));
	}
    } catch (...) {
	thread_label_cache_invalidate();
	throw;
    }
}

void
thread_drop_starpair(uint64_t h1, uint64_t h2)
{
    if (handle_debug)
	cprintf("[%ld] handle: dropping %ld, %ld\n", thread_id(), h1, h2);

    try {
	label clear;
	thread_cur_clearance(&clear);
	if (clear.get(h1) != clear.get_default() || clear.get(h2) != clear.get_default()) {
	    clear.set(h1, clear.get_default());
	    clear.set(h2, clear.get_default());
	    error_check(thread_set_clearance(&clear));
	}

	label self;
	thread_cur_label(&self);
	if (self.get(h1) != self.get_default() || self.get(h2) != self.get_default()) {
	    self.set(h1, self.get_default());
	    self.set(h2, self.get_default());
	    error_check(thread_set_label(&self));
	}
    } catch (...) {
	thread_label_cache_invalidate();
	throw;
    }
}

void
thread_label_cache_invalidate(void)
{
    scoped_jthread_lock x(&label_ops_mu);

    if (cur_th_clear_id == thread_id())
	cur_th_clear_id = 0;
    if (cur_th_label_id == thread_id())
	cur_th_label_id = 0;
}

void
get_label_retry(label *l, int (*fn) (struct ulabel *))
{
    int r;
    do {
	r = fn(l->to_ulabel());
	if (r == -E_NO_SPACE)
	    l->grow();
	else if (r < 0)
	    throw error(r, "getting label");
    } while (r == -E_NO_SPACE);
}

void
get_label_retry_obj(label *l, int (*fn) (struct cobj_ref, struct ulabel *), struct cobj_ref o)
{
    int r;
    do {
	r = fn(o, l->to_ulabel());
	if (r == -E_NO_SPACE)
	    l->grow();
	else if (r < 0)
	    throw error(r, "getting object label");
    } while (r == -E_NO_SPACE);
}

void
thread_cur_label(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);

    if (cur_th_label_id == thread_id()) {
	l->copy_from(&cur_th_label);
    } else {
	get_label_retry(l, thread_get_label);
	cur_th_label.copy_from(l);
	cur_th_label_id = thread_id();
    }
}

void
thread_cur_clearance(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);

    if (cur_th_clear_id == thread_id()) {
	l->copy_from(&cur_th_clear);
    } else {
	get_label_retry(l, &sys_self_get_clearance);
	cur_th_clear.copy_from(l);
	cur_th_clear_id = thread_id();
    }
}

void
thread_label_cache_update(label *l, label *c)
{
    scoped_jthread_lock x(&label_ops_mu);

    if (cur_th_label_id == thread_id())
	cur_th_label.copy_from(l);
    if (cur_th_clear_id == thread_id())
	cur_th_clear.copy_from(c);
}

void
thread_cur_verify(label *l, label *c)
{
    int r;
    do {
	r = sys_self_get_verify(l->to_ulabel(), c->to_ulabel());
	if (r == -E_NO_SPACE) {
	    l->grow();
	    c->grow();
	} else if (r < 0)
	    throw error(r, "getting label");
    } while (r == -E_NO_SPACE);
}

void
obj_get_label(struct cobj_ref o, label *l)
{
    get_label_retry_obj(l, &sys_obj_get_label, o);
}

void
gate_get_clearance(struct cobj_ref o, label *l)
{
    get_label_retry_obj(l, &sys_gate_clearance, o);
}

int64_t
handle_alloc(void)
{
    scoped_jthread_lock x(&label_ops_mu);

    int64_t h = sys_handle_create();
    if (h < 0)
	return h;

    if (cur_th_label_id == thread_id()) {
	try {
	    cur_th_label.set(h, LB_LEVEL_STAR);
	} catch (...) {
	    cur_th_label_id = 0;
	}
    }

    if (cur_th_clear_id == thread_id()) {
	try {
	    cur_th_clear.set(h, 3);
	} catch (...) {
	    cur_th_clear_id = 0;
	}
    }

    if (handle_debug)
	cprintf("[%ld] handle: allocated %ld\n", thread_id(), h);

    return h;
}
