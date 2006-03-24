extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
#include <inc/taint.h>
#include <inc/memlayout.h>
#include <inc/gateparam.h>

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
return_setup(cobj_ref *g, jos_jmp_buf *jb, uint64_t return_handle)
{
    label clear;
    thread_cur_clearance(&clear);
    clear.set(return_handle, 0);

    label label;
    thread_cur_label(&label);

    thread_entry te;
    te.te_entry = (void *) &return_stub;
    te.te_stack = (char *) tls_stack_top - 8;
    te.te_arg = (uint64_t) jb;
    error_check(sys_self_get_as(&te.te_as));

    uint64_t ct = kobject_id_thread_ct;
    int64_t id = sys_gate_create(ct, &te,
				 clear.to_ulabel(),
				 label.to_ulabel(),
				 "return gate");
    if (id < 0)
	throw error(id, "return_setup: creating return gate");

    *g = COBJ(ct, id);
}

void
gate_call(cobj_ref gate, gate_call_data *gcd_param,
	  label *cs, label *ds, label *dr)
{
    int64_t return_handle = sys_handle_create();
    error_check(return_handle);
    scope_guard<void, uint64_t> g1(thread_drop_star, return_handle);

    cobj_ref return_gate;
    jos_jmp_buf back_from_call;
    return_setup(&return_gate, &back_from_call, return_handle);
    scope_guard<int, cobj_ref> g2(sys_obj_unref, return_gate);

    label new_ds(ds ? *ds : label());
    new_ds.set(return_handle, LB_LEVEL_STAR);

    // Compute the target labels
    label tgt_label, tgt_clear;
    gate_compute_labels(gate, cs, &new_ds, dr, &tgt_label, &tgt_clear);

    // Create an MLT-like container for tainted data to live in
    label taint_ct_label, thread_label;
    thread_cur_label(&thread_label);

    // XXX should grant a preset handle at *, so others cannot use it..
    thread_label.merge(&tgt_label, &taint_ct_label, label::max, label::leq_starlo);
    taint_ct_label.transform(label::star_to, taint_ct_label.get_default());

    int64_t taint_ct = sys_container_alloc(start_env->shared_container,
					   taint_ct_label.to_ulabel(),
					   "gate call taint");
    if (taint_ct < 0)
	throw error(taint_ct, "gate_call: creating tainted container");

    scope_guard<int, cobj_ref> taint_ct_cleanup
	(sys_obj_unref, COBJ(start_env->shared_container, taint_ct));

    // Gate call parameters
    gate_call_data *d = (gate_call_data *) tls_gate_args;
    if (gcd_param)
	memcpy(d, gcd_param, sizeof(*d));
    d->return_gate = return_gate;
    d->taint_container = taint_ct;

    // Flush delayed unmap segment mappings; if we come back tainted,
    // we won't be able to look at our in-memory delayed unmap cache.
    // Perhaps this should be fixed by allowing a RW-mapping to be
    // mapped RO if that's all that the labeling permits.
    segment_unmap_flush();

    // Off into the gate!
    if (jos_setjmp(&back_from_call) == 0)
	gate_invoke(gate, &tgt_label, &tgt_clear, 0, 0);

    // Restore cached thread ID, just to be safe
    if (tls_tidp)
	*tls_tidp = sys_self_id();

    // Copy back the arguments
    if (gcd_param)
	memcpy(gcd_param, d, sizeof(*d));
}
