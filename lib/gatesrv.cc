extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateparam.hh>
#include <inc/gateinvoke.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>

gatesrv::gatesrv(uint64_t gate_ct, const char *name,
		 label *label, label *clearance)
    : stackpages_(2), active_(0)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    tls_ = 0;
    error_check(segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &tls_, 0));
    scope_guard<int, void *> g(segment_unmap, tls_);

    // Designated initializers are not supported in g++
    struct thread_entry te;
    te.te_entry = (void *) &entry_tls_stub;
    te.te_stack = (char *) tls_ + PGSIZE;
    te.te_arg = (uint64_t) this;
    error_check(sys_thread_get_as(&te.te_as));

    int64_t gate_id = sys_gate_create(gate_ct, &te,
				      clearance->to_ulabel(),
				      label->to_ulabel(), name);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    gate_obj_ = COBJ(gate_ct, gate_id);
    g.dismiss();
}

gatesrv::~gatesrv()
{
    segment_unmap(tls_);
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
	sys_thread_yield();

    error_check(sys_thread_addref(entry_container_));
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
    f_(arg_, d->param, &ret);

    throw basic_exception("gatesrv::entry: function returned\n");
}

void
gatesrv_return::ret(struct cobj_ref param, label *cs, label *ds, label *dr)
{
    struct gate_call_data *d = (struct gate_call_data *) tls_;
    d->param = param;

    stack_switch((uint64_t) this, (uint64_t) cs, (uint64_t) ds, (uint64_t) dr,
		 (char *) tls_ + PGSIZE,
		 (void *) &ret_tls_stub);
}

void
gatesrv_return::ret_tls_stub(gatesrv_return *r, label *cs, label *ds, label *dr)
{
    try {
	r->ret_tls(cs, ds, dr);
    } catch (std::exception &e) {
	printf("gatesrv_return::ret_tls_stub: %s\n", e.what());
	thread_halt();
    }
}

void
gatesrv_return::ret_tls(label *cs, label *ds, label *dr)
{
    gate_invoke(rgate_, cs, ds, dr, &cleanup_stub, this);
}

void
gatesrv_return::cleanup_stub(label *cs, label *ds, label *dr, void *arg)
{
    gatesrv_return *r = (gatesrv_return *) arg;
    r->cleanup(cs, ds, dr);
}

void
gatesrv_return::cleanup(label *cs, label *ds, label *dr)
{
    delete cs;
    delete ds;
    delete dr;

    struct cobj_ref thread_self = COBJ(thread_ct_, thread_id());
    struct cobj_ref stackseg;
    error_check(segment_lookup(stack_, &stackseg, 0));
    error_check(segment_unmap(stack_));
    error_check(sys_obj_unref(stackseg));
    error_check(sys_obj_unref(thread_self));
}
