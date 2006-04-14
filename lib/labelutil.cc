extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>

void
thread_drop_star(uint64_t handle)
{
    label clear;
    thread_cur_clearance(&clear);
    if (clear.get(handle) != clear.get_default()) {
	clear.set(handle, clear.get_default());
	error_check(sys_self_set_clearance(clear.to_ulabel()));
    }

    label self;
    thread_cur_label(&self);
    if (self.get(handle) != self.get_default()) {
	self.set(handle, self.get_default());
	error_check(sys_self_set_label(self.to_ulabel()));
    }
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
    get_label_retry(l, thread_get_label);
}

void
thread_cur_clearance(label *l)
{
    get_label_retry(l, &sys_self_get_clearance);
}

void
thread_cur_verify(label *l)
{
    get_label_retry(l, &sys_self_get_verify);
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
