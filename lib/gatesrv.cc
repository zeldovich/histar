extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/taint.h>

#include <stdio.h>
#include <string.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateinvoke.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
gatesrv_entry(gatesrv_entry_t fn, void *arg, void *stack, uint64_t flags)
{
    try {
	// Arguments for gate call passed on the top of the TLS stack.
	gate_call_data *d = (gate_call_data *) tls_gate_args;

	gatesrv_return ret(d->return_gate, start_env->proc_container,
			   d->taint_container, stack, flags);
	fn(arg, d, &ret);

	throw basic_exception("gatesrv_entry: function returned\n");
    } catch (std::exception &e) {
	// XXX need to clean up thread stack & refcount
	printf("gatesrv_entry: %s\n", e.what());
	thread_halt();
    }
}

static void __attribute__((noreturn))
gatesrv_entry_tls(gatesrv_entry_t fn, void *arg, uint64_t flags)
{
    try {
	// Copy-on-write if we are tainted
	gate_call_data *gcd = (gate_call_data *) TLS_GATE_ARGS;
	taint_cow(gcd->taint_container, gcd->declassify_gate);

	// Reset our cached thread ID, stored in TLS
	if (tls_tidp)
	    *tls_tidp = sys_self_id();

	thread_label_cache_invalidate();

	uint64_t entry_ct = start_env->proc_container;
	error_check(sys_self_set_sched_parents(gcd->taint_container, entry_ct));
	if (!(flags & GATESRV_NO_THREAD_ADDREF))
	    error_check(sys_self_addref(entry_ct));
	scope_guard<int, struct cobj_ref>
	    g(sys_obj_unref, COBJ(entry_ct, thread_id()));

	if ((flags & GATESRV_KEEP_TLS_STACK)) {
	    gatesrv_entry(fn, arg, 0, flags);
	} else {
	    void *stackbase = 0;
	    uint64_t stackbytes = thread_stack_pages * PGSIZE;
	    error_check(segment_map(COBJ(0, 0), 0, SEGMAP_STACK | SEGMAP_RESERVE,
				    &stackbase, &stackbytes, 0));
	    scope_guard<int, void *> unmap(segment_unmap, stackbase);
	    char *stacktop = ((char *) stackbase) + stackbytes;

	    struct cobj_ref stackobj;
	    void *allocbase = stacktop - PGSIZE;
	    error_check(segment_alloc(entry_ct, PGSIZE, &stackobj,
				      &allocbase, 0, "gate thread stack"));

	    g.dismiss();
	    unmap.dismiss();

	    stack_switch((uint64_t) fn, (uint64_t) arg, (uint64_t) stackbase, flags,
			 stacktop, (void *) &gatesrv_entry);
	}
    } catch (std::exception &e) {
	printf("gatesrv_entry_tls: %s\n", e.what());
	thread_halt();
    }
}

struct cobj_ref
gate_create(uint64_t gate_ct, const char *name,
	    label *label, label *clearance,
	    gatesrv_entry_t func, void *arg)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = gate_ct;
    gd.name_ = name;
    gd.label_ = label;
    gd.clearance_ = clearance;
    gd.func_ = func;
    gd.arg_ = arg;

    return gate_create(&gd);
}

struct cobj_ref
gate_create(gatesrv_descriptor *gd)
{
    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &gatesrv_entry_tls;
    te.te_stack = (char *) tls_stack_top - 8;
    te.te_arg[0] = (uint64_t) gd->func_;
    te.te_arg[1] = (uint64_t) gd->arg_;
    te.te_arg[2] = gd->flags_;
    te.te_as = gd->as_;

    if (te.te_as.object == 0)
	error_check(sys_self_get_as(&te.te_as));

    int64_t gate_id = sys_gate_create(gd->gate_container_, &te,
				      gd->clearance_->to_ulabel(),
				      gd->label_->to_ulabel(), gd->name_, 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    return COBJ(gd->gate_container_, gate_id);
}

void
gatesrv_return::ret(label *cs, label *ds, label *dr)
{
    error_check(sys_self_set_sched_parents(thread_ct_, gatecall_ct_));
    if ((flags_ & GATESRV_NO_THREAD_ADDREF))
	error_check(sys_self_addref(thread_ct_));

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

	gate_call_data *gcd = (gate_call_data *) tls_gate_args;
	int64_t id = sys_container_alloc(gcd->taint_container,
					 taint_ct_label.to_ulabel(),
					 "gate return taint", 0, CT_QUOTA_INF);
	if (id < 0) {
	    cprintf("gatesrv_return: allocating taint container in %ld: %s\n",
		    gcd->taint_container, e2s(id));
	    gcd->taint_container = 0;
	} else {
	    gcd->taint_container = id;
	}

	label verify(3);
	sys_self_set_verify(verify.to_ulabel());
    }

    if (!(flags_ & GATESRV_KEEP_TLS_STACK))
	stack_switch((uint64_t) this, (uint64_t) tgt_label, (uint64_t) tgt_clear, 0,
		     tls_stack_top, (void *) &ret_tls_stub);
    else
	ret_tls(tgt_label, tgt_clear);
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
    if (stack_) {
	char *stacktop = ((char *) stack_) + thread_stack_pages * PGSIZE;

	struct u_segment_mapping usm;
	error_check(segment_lookup_skip(stacktop - PGSIZE, &usm, SEGMAP_RESERVE));

	struct cobj_ref stackseg = usm.segment;
	error_check(segment_unmap_range(stack_, stacktop, 1));
	error_check(sys_obj_unref(stackseg));
    }
    error_check(sys_obj_unref(thread_self));
}
