extern "C" {
#include <inc/gateparam.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/error.h>
}

#include <crypt.h>
#include <dj/gateexec.hh>
#include <dj/djgate.h>
#include <dj/reqcontext.hh>
#include <dj/catmgr.hh>
#include <dj/djlabel.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/gateinvoke.hh>

struct gate_exec_thread_state {
    cobj_ref gate;
    uint64_t msg_ct;
    uint64_t msg_id;

    label tgt_l, tgt_c;
    label vl, vc;

    uint64_t done;
};

static void
gate_exec_invoke_cb(label *tgt_l, label *tgt_c, void *arg)
{
    gate_exec_thread_state *s = (gate_exec_thread_state *) arg;

    uint64_t tid = sys_self_id();
    int r = sys_self_addref(s->msg_ct);
    if (r < 0)
	cprintf("gate_exec_invoke_cb: sys_self_addref: %s\n", e2s(r));

    r = sys_self_set_sched_parents(start_env->proc_container, s->msg_ct);
    if (r < 0)
	cprintf("gate_exec_invoke_cb: set_sched_parents: %s\n", e2s(r));

    s->done = 1;
    r = sys_sync_wakeup(&s->done);
    if (r < 0)
	cprintf("gate_exec_invoke_cb: sys_sync_wakeup: %s\n", e2s(r));

    r = sys_obj_unref(COBJ(start_env->proc_container, tid));
    if (r < 0)
	cprintf("gate_exec_invoke_cb: sys_obj_unref: %s\n", e2s(r));
}

static void __attribute__((noreturn))
gate_exec_thread(gate_exec_thread_state *s)
{
    gate_call_data *gcd = (gate_call_data *) tls_gate_args;
    gcd->taint_container = s->msg_ct;	/* for set_sched_parents */
    gcd->param_obj = COBJ(s->msg_ct, s->msg_id);

    sys_self_set_verify(s->vl.to_ulabel(), s->vc.to_ulabel());
    gate_invoke(s->gate, &s->tgt_l, &s->tgt_c, &gate_exec_invoke_cb, s);
}

static void
gate_exec2(catmgr *cm, const dj_pubkey &sender,
	   const dj_message &m, const delivery_args &da)
{
    if (m.target.type != EP_GATE)
	throw basic_exception("gate_exec only does gates");

    /*
     * Translate the global labels into local ones.
     */
    label msg_taint, msg_glabel, msg_gclear;

    try {
	dj_catmap_indexed cmi(m.catmap);
	djlabel_to_label(cmi, m.taint,  &msg_taint);
	djlabel_to_label(cmi, m.glabel, &msg_glabel);
	djlabel_to_label(cmi, m.gclear, &msg_gclear);
    } catch (std::exception &e) {
	warn << "gate_exec2: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING, 0);
	return;
    }

    /*
     * Compute the verify/grant label and clearance.
     */
    gate_exec_thread_state s;
    s.done = 0;
    s.gate = COBJ(m.target.gate->gate_ct, m.target.gate->gate_id);
    s.msg_ct = m.msg_ct;

    msg_taint.merge(&msg_glabel, &s.vl, label::min, label::leq_starlo);
    msg_taint.merge(&msg_gclear, &s.vc, label::max, label::leq_starlo);

    verify_label_reqctx ctx(s.vl, s.vc);

    /*
     * Acquire whatever resources the caller wants..
     */
    try {
	cm->acquire(m.catmap);
	cm->resource_check(&ctx, m.catmap);
    } catch (std::exception &e) {
	warn << "gate_exec2: acquiring: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING, 0);
	return;
    }

    /*
     * Compute target gate labels
     */
    gate_compute_labels(s.gate, &msg_taint, &s.vl, &s.vc, &s.tgt_l, &s.tgt_c);

    /*
     * Write message to segment
     */
    dj_outgoing_gate_msg gmsg;
    gmsg.sender = sender;
    gmsg.m = m;
    str gmstr = xdr2str(gmsg);

    if (!ctx.can_rw(COBJ(m.msg_ct, m.msg_ct)))
	throw basic_exception("caller cannot write container %ld", m.msg_ct);

    cobj_ref mseg;
    void *data_map = 0;
    error_check(segment_alloc(m.msg_ct, gmstr.len(), &mseg,
			      &data_map, msg_taint.to_ulabel(),
			      "gate_exec message"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    scope_guard<int, cobj_ref> unref1(sys_obj_unref, mseg);
    memcpy(data_map, gmstr.cstr(), gmstr.len());
    s.msg_id = mseg.object;

    /*
     * Start the thread to call the gate
     */
    thread_entry te;
    te.te_entry = (void *) &gate_exec_thread;
    te.te_stack = tls_stack_top;
    te.te_arg[0] = (uintptr_t) &s;
    error_check(sys_self_get_as(&te.te_as));

    uint64_t pct = start_env->proc_container;
    int64_t tid = sys_thread_create(pct, "gate_exec");
    error_check(tid);
    scope_guard<int, cobj_ref> unref2(sys_obj_unref, COBJ(pct, tid));

    error_check(sys_container_move_quota(pct, tid, thread_quota_slush));
    error_check(sys_obj_set_fixedquota(COBJ(pct, tid)));
    error_check(sys_thread_start(COBJ(pct, tid), &te, 0, 0));

    while (s.done == 0)
	sys_sync_wait(&s.done, 0, ~0UL);
    da.cb(DELIVERY_DONE, tid);

    unref1.dismiss();
    unref2.dismiss();
}

void
gate_exec(catmgr *cm, const dj_pubkey &node,
	  const dj_message &m, const delivery_args &da)
{
    try {
	gate_exec2(cm, node, m, da);
    } catch (std::exception &e) {
	warn << "gate_exec: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_ERR, 0);
    }
}
