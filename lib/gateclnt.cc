extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
#include <inc/taint.h>
#include <inc/memlayout.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
return_stub(jos_jmp_buf *jb)
{
    // Note: cannot use tls_gate_args variable since it's in RW-mapped space
    gate_call_data *gcd = (gate_call_data *) TLS_GATE_ARGS;
    taint_cow(gcd->taint_container, gcd->declassify_gate);
    jos_longjmp(jb, 1);
}

static void
return_setup(cobj_ref *g, jos_jmp_buf *jb, uint64_t return_handle, uint64_t ct)
{
    label clear;
    thread_cur_clearance(&clear);

    label cur_label;
    thread_cur_label(&cur_label);

    label max_clear(cur_label);
    max_clear.transform(label::star_to, 3);

    label out;
    clear.merge(&max_clear, &out, label::max, label::leq_starlo);
    clear.copy_from(&out);
    clear.set(return_handle, 0);

    thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &return_stub;
    te.te_stack = (char *) tls_stack_top - 8;
    te.te_arg[0] = (uint64_t) jb;
    error_check(sys_self_get_as(&te.te_as));

    int64_t id = sys_gate_create(ct, &te,
				 clear.to_ulabel(),
				 cur_label.to_ulabel(),
				 "return gate");
    if (id < 0)
	throw error(id, "return_setup: creating return gate");

    *g = COBJ(ct, id);
}

gate_call::gate_call(cobj_ref gate,
		     label *cs, label *ds, label *dr)
    : gate_(gate)
{
    // Create a handle for the duration of this call
    error_check(call_handle_ = sys_handle_create());
    scope_guard<void, uint64_t> drop_star(thread_drop_star, call_handle_);

    // Compute the target labels
    label new_ds(ds ? *ds : label(3));
    new_ds.set(call_handle_, LB_LEVEL_STAR);

    tgt_label_ = new label();
    scope_guard<void, label*> del_tl(delete_obj, tgt_label_);
    tgt_clear_ = new label();
    scope_guard<void, label*> del_tc(delete_obj, tgt_clear_);
    gate_compute_labels(gate, cs, &new_ds, dr, tgt_label_, tgt_clear_);

    // Create an MLT-like container for tainted data to live in
    obj_label_ = new label();
    scope_guard<void, label*> del_ol(delete_obj, obj_label_);

    label thread_label;
    thread_cur_label(&thread_label);
    thread_label.merge(tgt_label_, obj_label_, label::max, label::leq_starlo);
    obj_label_->transform(label::star_to, obj_label_->get_default());
    obj_label_->set(call_handle_, 0);

    // XXX should we actually be allocating a call_grant/call_taint handle pair?
    int64_t taint_ct = sys_container_alloc(start_env->shared_container,
					   obj_label_->to_ulabel(),
					   "gate call taint", 0, CT_QUOTA_INF);
    if (taint_ct < 0)
	throw error(taint_ct, "gate_call: creating tainted container");

    taint_ct_obj_ = COBJ(start_env->shared_container, taint_ct);
    scope_guard<int, cobj_ref> taint_ct_cleanup(sys_obj_unref, taint_ct_obj_);

    // Let the destructor clean up after this point
    drop_star.dismiss();
    del_tl.dismiss();
    del_tc.dismiss();
    del_ol.dismiss();
    taint_ct_cleanup.dismiss();
}

void
gate_call::call(gate_call_data *gcd_param, label *verify)
{
    // Set the verify label; prove we had the call handle at *
    label new_verify(verify ? *verify : label(3));
    new_verify.set(call_handle_, LB_LEVEL_STAR);
    sys_self_set_verify(new_verify.to_ulabel());

    // Create a return gate in the taint container
    cobj_ref return_gate;
    jos_jmp_buf back_from_call;
    return_setup(&return_gate, &back_from_call, call_handle_, taint_ct_obj_.object);
    scope_guard<int, cobj_ref> g2(sys_obj_unref, return_gate);

    // Gate call parameters
    gate_call_data *d = (gate_call_data *) tls_gate_args;
    if (gcd_param)
	memcpy(d, gcd_param, sizeof(*d));
    d->return_gate = return_gate;
    d->taint_container = taint_ct_obj_.object;

    // Flush delayed unmap segment mappings; if we come back tainted,
    // we won't be able to look at our in-memory delayed unmap cache.
    // Perhaps this should be fixed by allowing a RW-mapping to be
    // mapped RO if that's all that the labeling permits.
    segment_unmap_flush();

    // Off into the gate!
    if (jos_setjmp(&back_from_call) == 0)
	gate_invoke(gate_, tgt_label_, tgt_clear_, 0, 0);

    // Restore cached thread ID, just to be safe
    if (tls_tidp)
	*tls_tidp = sys_self_id();

    // Copy back the arguments
    if (gcd_param)
	memcpy(gcd_param, d, sizeof(*d));
}

gate_call::~gate_call()
{
    try {
	sys_obj_unref(taint_ct_obj_);

	if (tgt_label_)
	    delete tgt_label_;
	if (tgt_clear_)
	    delete tgt_clear_;
	if (obj_label_)
	    delete obj_label_;

	thread_drop_star(call_handle_);
    } catch (std::exception &e) {
	cprintf("gate_call::~gate_call: %s\n", e.what());
    }
}
