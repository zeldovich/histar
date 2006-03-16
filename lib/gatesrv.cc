extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
#include <stdio.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateparam.hh>
#include <inc/gateinvoke.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

gatesrv::gatesrv(uint64_t gate_ct, const char *name,
		 label *label, label *clearance)
    : tls_((void *) UTLS), stackpages_(2), active_(0)
{
    // Designated initializers are not supported in g++
    struct thread_entry te;
    te.te_entry = (void *) &entry_tls_stub;
    te.te_stack = (char *) tls_ + PGSIZE - 8;
    te.te_arg = (uint64_t) this;
    error_check(sys_self_get_as(&te.te_as));

    int64_t gate_id = sys_gate_create(gate_ct, &te,
				      clearance->to_ulabel(),
				      label->to_ulabel(), name);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    gate_obj_ = COBJ(gate_ct, gate_id);
}

gatesrv::~gatesrv()
{
    sys_obj_unref(gate_obj_);
}

void
gatesrv::entry_tls_stub(gatesrv *s)
{
    try {
	s->entry_tls();
    } catch (std::exception &e) {
	printf("gatesrv::entry_tls_stub: %s\n", e.what());
	thread_halt();
    }
}

void
gatesrv::entry_tls()
{
    while (!active_)
	sys_self_yield();

    error_check(sys_self_addref(entry_container_));
    scope_guard<int, struct cobj_ref>
	g(sys_obj_unref, COBJ(entry_container_, thread_id()));

    struct cobj_ref stackobj;
    void *stack = 0;
    error_check(segment_alloc(entry_container_, stackpages_ * PGSIZE,
			      &stackobj, &stack, 0, "gate thread stack"));
    g.dismiss();

    stack_switch((uint64_t) this, (uint64_t) stack, 0, 0,
		 (char *) stack + stackpages_ * PGSIZE,
		 (void *) &entry_stub);
}

void
gatesrv::entry_stub(gatesrv *s, void *stack)
{
    try {
	s->entry(stack);
    } catch (std::exception &e) {
	printf("gatesrv::entry_stub: %s\n", e.what());
	thread_halt();
    }
}

void
gatesrv::entry(void *stack)
{
    // Arguments for gate call passed on the bottom of the tls
    struct gate_call_data *d = (struct gate_call_data *) tls_;

    gatesrv_return ret(d->return_gate, entry_container_, tls_, stack);
    f_(arg_, d, &ret);

    throw basic_exception("gatesrv::entry: function returned\n");
}

void
gatesrv_return::ret(label *cs, label *ds, label *dr)
{
    label *tgt_label = new label();
    label *tgt_clear = new label();

    gate_compute_labels(rgate_, cs, ds, dr, tgt_label, tgt_clear);

    if (cs)
	delete cs;
    if (ds)
	delete ds;
    if (dr)
	delete dr;

    // Create an MLT-like container for tainted data.
    // New scope to free thread_label and taint_ct_label.
    {
	label taint_ct_label, thread_label;
	thread_cur_label(&thread_label);

	thread_label.merge(tgt_label, &taint_ct_label, label::max, label::leq_starlo);
	taint_ct_label.transform(label::star_to, taint_ct_label.get_default());

	gate_call_data *gcd = (gate_call_data *) tls_;
	int64_t id = sys_container_alloc(gcd->taint_container,
					 taint_ct_label.to_ulabel(),
					 "gate return taint");
	if (id < 0)
	    throw error(id, "gatesrv_return: allocating taint container");

	gcd->taint_container = id;
    }

    stack_switch((uint64_t) this, (uint64_t) tgt_label, (uint64_t) tgt_clear, 0,
		 (char *) tls_ + PGSIZE,
		 (void *) &ret_tls_stub);
}

void
gatesrv_return::ret_tls_stub(gatesrv_return *r, label *tgt_label, label *tgt_clear)
{
    try {
	r->ret_tls(tgt_label, tgt_clear);
    } catch (std::exception &e) {
	printf("gatesrv_return::ret_tls_stub: %s\n", e.what());
	thread_halt();
    }
}

void
gatesrv_return::ret_tls(label *tgt_label, label *tgt_clear)
{
    gate_invoke(rgate_, tgt_label, tgt_clear, &cleanup_stub, this);
}

void
gatesrv_return::cleanup_stub(label *tgt_s, label *tgt_r, void *arg)
{
    gatesrv_return *r = (gatesrv_return *) arg;
    r->cleanup(tgt_s, tgt_r);
}

void
gatesrv_return::cleanup(label *tgt_s, label *tgt_r)
{
    delete tgt_s;
    delete tgt_r;

    struct cobj_ref thread_self = COBJ(thread_ct_, thread_id());
    struct cobj_ref stackseg;
    error_check(segment_lookup(stack_, &stackseg, 0, 0));
    error_check(segment_unmap(stack_));
    error_check(sys_obj_unref(stackseg));
    error_check(sys_obj_unref(thread_self));
}
