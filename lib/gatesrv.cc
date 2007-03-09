extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/taint.h>
#include <inc/error.h>

#include <stdio.h>
#include <string.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateinvoke.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

enum { gatesrv_debug = 0 };

static void __attribute__((noreturn))
gatesrv_cleanup_tls(void *stack, uint64_t thread_ref_ct)
{
    if (stack) {
	struct u_segment_mapping usm;
	int r = segment_lookup(stack, &usm);
	if (r < 0) {
	    printf("gatesrv_cleanup_tls: cannot lookup stack: %s\n", e2s(r));
	} else {
	    segment_unmap_delayed(stack, 1);
	    sys_obj_unref(usm.segment);
	}
    }

    uint64_t tid = sys_self_id();

    sys_self_set_sched_parents(start_env->proc_container, thread_ref_ct);
    sys_obj_unref(COBJ(start_env->proc_container, tid));
    sys_obj_unref(COBJ(thread_ref_ct, tid));
    thread_halt();
}

static void __attribute__((noreturn))
gatesrv_entry(gatesrv_entry_t fn, void *arg, void *stack, uint64_t flags)
{
    // Arguments for gate call passed on the top of the TLS stack.
    gate_call_data *d = (gate_call_data *) tls_gate_args;
    uint64_t thread_ref_ct = d->thread_ref_ct;

    try {
	gatesrv_return ret(d->return_gate, start_env->proc_container,
			   d->thread_ref_ct, stack, flags);
	fn(arg, d, &ret);
    } catch (std::exception &e) {
	printf("gatesrv_entry: %s\n", e.what());
    }

    if (flags & GATESRV_NO_THREAD_ADDREF)
	thread_halt();

    stack_switch((uint64_t) stack, thread_ref_ct, 0, 0,
		 tls_stack_top, (void *) &gatesrv_cleanup_tls);
}

static void __attribute__((noreturn))
gatesrv_entry_tls(gatesrv_entry_t fn, void *arg, uint64_t flags)
{
    try {
	// Copy-on-write if we are tainted
	gate_call_data *gcd = (gate_call_data *) TLS_GATE_ARGS;
	int did_taint = taint_cow(gcd->taint_container, gcd->declassify_gate);

	// Reset our cached thread ID, stored in TLS
	tls_revalidate();

	if (gatesrv_debug)
	    cprintf("[%ld] gatesrv_entry_tls\n", thread_id());

	thread_label_cache_invalidate();

	uint64_t entry_ct = start_env->proc_container;
	error_check(sys_self_set_sched_parents(gcd->thread_ref_ct, entry_ct));

	/* taint_cow() already addref's the thread into the new proc_container */
	if (!(flags & GATESRV_NO_THREAD_ADDREF) && !did_taint)
	    error_check(sys_self_addref(entry_ct));
	scope_guard<int, cobj_ref>
	    g(sys_obj_unref, COBJ(entry_ct, thread_id()));

	if ((flags & GATESRV_KEEP_TLS_STACK)) {
	    gatesrv_entry(fn, arg, 0, flags);
	} else {
	    struct cobj_ref stackobj;
	    error_check(segment_alloc(entry_ct, PGSIZE, &stackobj,
				      0, 0, "gate thread stack"));
	    scope_guard<int, cobj_ref> s(sys_obj_unref, stackobj);

	    void *stackbase = 0;
	    uint64_t stackbytes = thread_stack_pages * PGSIZE;
	    error_check(segment_map(stackobj, 0, SEGMAP_READ | SEGMAP_WRITE |
				    SEGMAP_STACK | SEGMAP_REVERSE_PAGES,
				    &stackbase, &stackbytes, 0));
	    char *stacktop = ((char *) stackbase) + stackbytes;

	    g.dismiss();
	    s.dismiss();

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
	    label *label, label *clearance, label *verify,
	    gatesrv_entry_t func, void *arg)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = gate_ct;
    gd.name_ = name;
    gd.label_ = label;
    gd.clearance_ = clearance;
    gd.verify_ = verify;
    gd.func_ = func;
    gd.arg_ = arg;

    return gate_create(&gd);
}

struct cobj_ref
gate_create(gatesrv_descriptor *gd)
{
    if ((gd->flags_ & GATESRV_NO_THREAD_ADDREF) &&
	!(gd->flags_ & GATESRV_KEEP_TLS_STACK))
    {
	fprintf(stderr, "gate_create: must KEEP_TLS_STACK if NO_THREAD_ADDREF\n");
	throw basic_exception("gate_create: bad flags");
    }

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
				      gd->label_ ? gd->label_->to_ulabel() : 0,
				      gd->clearance_ ? gd->clearance_->to_ulabel() : 0,
				      gd->verify_ ? gd->verify_->to_ulabel() : 0,
				      gd->name_, 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    return COBJ(gd->gate_container_, gate_id);
}

void
gatesrv_return::ret(label *cs, label *ds, label *dr, label *vl, label *vc)
{
    error_check(sys_self_set_sched_parents(thread_ct_, gate_tref_ct_));
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

    { // GC scope
	label blank_vl(3), blank_vc(0);
	if (!vl)
	    vl = &blank_vl;
	if (!vc)
	    vc = &blank_vc;

	error_check(sys_self_set_verify(vl->to_ulabel(), vc->to_ulabel()));
	if (vl != &blank_vl)
	    delete vl;
	if (vc != &blank_vc)
	    delete vc;

	label taint_ct_label, thread_label;
	thread_cur_label(&thread_label);

	thread_label.merge(tgt_label, &taint_ct_label, label::max, label::leq_starlo);
	taint_ct_label.transform(label::star_to, taint_ct_label.get_default());

	gate_call_data *gcd = (gate_call_data *) tls_gate_args;
	int64_t id = sys_container_alloc(gcd->taint_container,
					 taint_ct_label.to_ulabel(),
					 "gate return taint", 0, CT_QUOTA_INF);
	if (id < 0) {
	    // Usually -E_INVAL means the caller has died, so it doesn't matter..
	    if (id != -E_INVAL)
		cprintf("gatesrv_return: allocating taint container in %ld: %s\n",
			gcd->taint_container, e2s(id));
	    gcd->taint_container = 0;
	} else {
	    gcd->taint_container = id;
	}
	gcd->thread_ref_ct = gate_tref_ct_;
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
	if (gatesrv_debug)
	    cprintf("[%ld] gatesrv_return::ret_tls_stub\n", thread_id());
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
    if (gatesrv_debug)
	cprintf("[%ld] gatesrv_return::cleanup_stub\n", thread_id());

    gatesrv_return *r = (gatesrv_return *) arg;
    r->cleanup(tgt_s, tgt_r);

    if (gatesrv_debug)
	cprintf("[%ld] gatesrv_return::cleanup_stub done\n", thread_id());
}

void
gatesrv_return::cleanup(label *tgt_s, label *tgt_r)
{
    delete tgt_s;
    delete tgt_r;

    struct cobj_ref thread_self = COBJ(thread_ct_, thread_id());
    if (stack_) {
	struct u_segment_mapping usm;
	error_check(segment_lookup(stack_, &usm));

	struct cobj_ref stackseg = usm.segment;
	error_check(segment_unmap_delayed(stack_, 1));
	error_check(sys_obj_unref(stackseg));
    }
    error_check(sys_obj_unref(thread_self));
}
