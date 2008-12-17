extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inttypes.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/jthread.hh>

static jthread_mutex_t label_ops_mu;
static uint64_t cur_th_label_id, cur_th_owner_id, cur_th_clear_id;
static label *cur_th_label, *cur_th_owner, *cur_th_clear;

enum { category_debug = 0 };

static void
label_cache_init(void)
{
    if (!cur_th_label)
	cur_th_label = new label();
    if (!cur_th_owner)
	cur_th_owner = new label();
    if (!cur_th_clear)
	cur_th_clear = new label();
}

int
thread_set_label(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    int r = sys_self_set_label(l->to_ulabel());
    if (r < 0)
	return r;

    *cur_th_label = *l;
    cur_th_label_id = thread_id();
    return 0;
}

int
thread_set_ownership(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    int r = sys_self_set_ownership(l->to_ulabel());
    if (r < 0)
	return r;

    *cur_th_owner = *l;
    cur_th_owner_id = thread_id();
    return 0;
}

int
thread_set_clearance(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    int r = sys_self_set_clearance(l->to_ulabel());
    if (r < 0)
	return r;

    *cur_th_clear = *l;
    cur_th_clear_id = thread_id();
    return 0;
}

void
thread_drop_star(uint64_t cat)
{
    if (category_debug)
	cprintf("[%"PRIu64"] category: dropping %"PRIu64"\n", thread_id(), cat);

    try {
	label o;
	thread_cur_ownership(&o);
	if (o.contains(cat)) {
	    o.remove(cat);
	    error_check(thread_set_ownership(&o));
	}
    } catch (...) {
	thread_label_cache_invalidate();
	throw;
    }
}

void
thread_drop_starpair(uint64_t c1, uint64_t c2)
{
    if (category_debug)
	cprintf("[%"PRIu64"] category: dropping %"PRIu64", %"PRIu64"\n", thread_id(), c1, c2);

    try {
	label o;
	thread_cur_label(&o);
	if (o.contains(h1) || o.contains(h2)) {
	    o.remove(h1);
	    o.remove(h2);
	    error_check(thread_set_ownership(&o));
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
    label_cache_init();

    if (cur_th_label_id == thread_id()) {
	*l = *cur_th_label;
    } else {
	get_label_retry(l, thread_get_label);
	*cur_th_label = *l;
	cur_th_label_id = thread_id();
    }
}

void
thread_cur_clearance(label *l)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    if (cur_th_clear_id == thread_id()) {
	*l = *cur_th_clear;
    } else {
	get_label_retry(l, &sys_self_get_clearance);
	*cur_th_clear = *l;
	cur_th_clear_id = thread_id();
    }
}

void
thread_label_cache_update(label *l, label *c)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    if (cur_th_label_id == thread_id())
	*cur_th_label = *l;
    if (cur_th_clear_id == thread_id())
	*cur_th_clear = *c;
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
category_alloc(int secrecy)
{
    scoped_jthread_lock x(&label_ops_mu);
    label_cache_init();

    int64_t c = sys_category_alloc(secrecy);
    if (c < 0)
	return c;

    if (cur_th_owner_id == thread_id()) {
	try {
	    cur_th_owner->add(c);
	} catch (...) {
	    cur_th_owner_id = 0;
	}
    }

    if (category_debug)
	cprintf("[%"PRIu64"] category: allocated %"PRIu64"\n", thread_id(), c);

    return c;
}
