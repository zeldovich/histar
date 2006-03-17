extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
#include <inc/taint.h>
#include <inc/memlayout.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/gateparam.hh>
#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
return_stub(jos_jmp_buf *jb)
{
    gate_call_data *gcd = (gate_call_data *) UTLS;
    taint_cow(gcd->taint_container);
    jos_longjmp(jb, 1);
}

static void
return_setup(cobj_ref *g, jos_jmp_buf *jb, char *tls, uint64_t return_handle)
{
    label clear;
    thread_cur_clearance(&clear);
    clear.set(return_handle, 0);

    label label;
    thread_cur_label(&label);

    thread_entry te;
    te.te_entry = (void *) &return_stub;
    te.te_stack = tls + PGSIZE - 8;
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
    char *tls = (char *) UTLS;

    int64_t return_handle = sys_handle_create();
    error_check(return_handle);
    scope_guard<void, uint64_t> g1(thread_drop_star, return_handle);

    cobj_ref return_gate;
    jos_jmp_buf back_from_call;
    return_setup(&return_gate, &back_from_call, tls, return_handle);
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

    // Gate call parameters; skip over 8-byte cached thread ID
    gate_call_data *d = (gate_call_data *) (tls + 8);
    if (gcd_param)
	memcpy(d, gcd_param, sizeof(*d));
    d->return_gate = return_gate;
    d->taint_container = taint_ct;

    // Off into the gate!
    if (jos_setjmp(&back_from_call) == 0)
	gate_invoke(gate, &tgt_label, &tgt_clear, 0, 0);

    // Restore cached thread ID, just to be safe
    uint64_t *tls_tidp = (uint64_t *) tls;
    *tls_tidp = sys_self_id();

    // Copy back the arguments
    if (gcd_param)
	memcpy(gcd_param, d, sizeof(*d));
}
