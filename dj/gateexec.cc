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
#include <dj/perf.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/gateinvoke.hh>

enum { gate_exec_debug = 0 };

struct gate_exec_thread_state {
    cobj_ref gate;
    uint64_t msg_ct;
    uint64_t msg_id;

    label tgt_l, tgt_c;
    label vl, vc;
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

    delete s;

    r = sys_obj_unref(COBJ(start_env->proc_container, tid));
    if (r < 0)
	cprintf("gate_exec_invoke_cb: sys_obj_unref: %s\n", e2s(r));
}

static void __attribute__((noreturn, regparm(1)))
gate_exec_thread(gate_exec_thread_state *s)
{
    gate_call_data *gcd = &tls_data->tls_gate_args;
    gcd->taint_container = s->msg_ct;
    gcd->thread_ref_ct = s->msg_ct;
    gcd->param_obj = COBJ(s->msg_ct, s->msg_id);

    sys_self_set_verify(s->vl.to_ulabel(), s->vc.to_ulabel());
    gate_invoke(s->gate, &s->tgt_l, &s->tgt_c, &gate_exec_invoke_cb, s);
}

static void
gate_exec2(catmgr *cm, const dj_message &m, const delivery_args &da,
	   const cobj_ref &djd_gate)
{
    PERF_COUNTER(gate_exec2);

    if (m.target.type != EP_GATE)
	throw basic_exception("gate_exec only does gates");

    if (gate_exec_debug) {
	warn << "gate_exec: delivering to " << m.target.ep_gate->msg_ct
	     << "." << m.target.ep_gate->gate.gate_ct
	     << "." << m.target.ep_gate->gate.gate_id << "\n";
	warn << "taint " << m.taint << "\n";
	warn << "grant " << m.glabel << "\n";
	warn << "clear " << m.gclear << "\n";
    }

    /*
     * Translate the global labels into local ones.
     */
    label msg_taint, msg_glabel, msg_gclear;

    try {
	dj_catmap_indexed cmi(m.catmap);
	djlabel_to_label(cmi, m.taint,  &msg_taint, label_taint);
	djlabel_to_label(cmi, m.glabel, &msg_glabel, label_owner);
	djlabel_to_label(cmi, m.gclear, &msg_gclear, label_clear);
    } catch (std::exception &e) {
	warn << "gate_exec2: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    /*
     * Compute the verify/grant label and clearance.
     */
    gate_exec_thread_state *s = New gate_exec_thread_state();
    scope_guard<void, gate_exec_thread_state*> delete_s(delete_obj, s);

    if (m.target.ep_gate->gate.gate_ct) {
	s->gate.container = m.target.ep_gate->gate.gate_ct;
	s->gate.object = m.target.ep_gate->gate.gate_id;
    } else {
	PERF_COUNTER(gate_exec2::specfind);

	uint64_t spec_id = m.target.ep_gate->gate.gate_id;
	const char *ctname = 0, *gtname = 0;

	if (spec_id == GSPEC_CTALLOC) {
	    ctname = "ctallocd";
	    gtname = "ctallocd";
	}

	if (spec_id == GSPEC_ECHO) {
	    ctname = "djechod";
	    gtname = "djechod";
	}

	if (spec_id == GSPEC_GUARDCALL) {
	    ctname = "djguardcall";
	    gtname = "djguardcall";
	}

	if (!ctname || !gtname) {
	    warn << "gate_exec2: bad special gate id " << spec_id << "\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	int64_t ct = container_find(start_env->root_container,
				    kobj_container, ctname);
	int64_t gt = container_find(ct, kobj_gate, gtname);
	if (ct < 0 || gt < 0) {
	    warn << "gate_exec2: cannot find spec gate " << spec_id << "\n";
	    da.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	s->gate.container = ct;
	s->gate.object = gt;
    }
    s->msg_ct = m.target.ep_gate->msg_ct;

    msg_taint.merge(&msg_glabel, &s->vl, label::min, label::leq_starlo);
    msg_taint.merge(&msg_gclear, &s->vc, label::max, label::leq_starlo);

    if (gate_exec_debug) {
	warn << "gate_exec: msg_taint  " << msg_taint.to_string() << "\n";
	warn << "gate_exec: msg_glabel " << msg_glabel.to_string() << "\n";
	warn << "gate_exec: msg_gclear " << msg_gclear.to_string() << "\n";
	warn << "gate_exec: verify lab " << s->vl.to_string() << "\n";
	warn << "gate_exec: verify clr " << s->vc.to_string() << "\n";
    }

    verify_label_reqctx ctx(s->vl, s->vc);

    /*
     * Acquire whatever resources the caller wants..
     */
    try {
	cm->acquire(m.catmap, true);
    } catch (std::exception &e) {
	warn << "gate_exec2: acquiring: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    /*
     * Compute target gate labels
     */
    gate_compute_labels(s->gate, &msg_taint, &s->vl, &s->vc, &s->tgt_l, &s->tgt_c);

    /*
     * Write message to segment
     */
    dj_outgoing_gate_msg gmsg;
    gmsg.djd_gate.gate_ct = djd_gate.container;
    gmsg.djd_gate.gate_id = djd_gate.object;
    gmsg.m = m;
    str gmstr = xdr2str(gmsg);

    if (!ctx.can_rw(COBJ(s->msg_ct, s->msg_ct)))
	throw basic_exception("caller cannot write container %ld", s->msg_ct);

    cobj_ref mseg;
    void *data_map = 0;
    error_check(segment_alloc(s->msg_ct, gmstr.len(), &mseg,
			      &data_map, msg_taint.to_ulabel(),
			      "gate_exec message"));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    scope_guard<int, cobj_ref> unref1(sys_obj_unref, mseg);
    memcpy(data_map, gmstr.cstr(), gmstr.len());
    s->msg_id = mseg.object;

    /*
     * Start the thread to call the gate
     */
    thread_entry te;
    te.te_entry = (void *) &gate_exec_thread;
    te.te_stack = tls_stack_top;
    te.te_arg[0] = (uintptr_t) s;
    error_check(sys_self_get_as(&te.te_as));

    uint64_t pct = start_env->proc_container;
    int64_t tid = sys_thread_create(pct, "gate_exec");
    error_check(tid);
    scope_guard<int, cobj_ref> unref2(sys_obj_unref, COBJ(pct, tid));

    error_check(sys_container_move_quota(pct, tid, thread_quota_slush));
    error_check(sys_obj_set_fixedquota(COBJ(pct, tid)));
    error_check(sys_thread_start(COBJ(pct, tid), &te, 0, 0));

    da.cb(DELIVERY_DONE);

    delete_s.dismiss();
    unref1.dismiss();
    unref2.dismiss();
}

void
gate_exec(catmgr *cm, cobj_ref djd_gate,
	  const dj_message &m, const delivery_args &da)
{
    try {
	gate_exec2(cm, m, da, djd_gate);
    } catch (std::exception &e) {
	warn << "gate_exec: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_ERR);
    }

    cm->drop_now();
}

static void
segment_exec2(catmgr *cm, const dj_message &m, const delivery_args &da)
{
    PERF_COUNTER(segment_exec2);

    if (m.target.type != EP_SEGMENT)
	throw basic_exception("segment_exec only does segments");

    /*
     * Translate the global labels into local ones.
     */
    label msg_taint, msg_glabel, msg_gclear;

    try {
	dj_catmap_indexed cmi(m.catmap);
	djlabel_to_label(cmi, m.taint,  &msg_taint, label_taint);
	djlabel_to_label(cmi, m.glabel, &msg_glabel, label_owner);
	djlabel_to_label(cmi, m.gclear, &msg_gclear, label_clear);
    } catch (std::exception &e) {
	warn << "segment_exec2: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    /*
     * Acquire whatever resources the caller wants..
     */
    try {
	cm->acquire(m.catmap, true);
    } catch (std::exception &e) {
	warn << "segment_exec2: acquiring: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_MAPPING);
	return;
    }

    /*
     * Make sure caller can write to this segment.
     */
    label vl, vc;
    msg_taint.merge(&msg_glabel, &vl, label::min, label::leq_starlo);
    msg_taint.merge(&msg_gclear, &vc, label::max, label::leq_starlo);

    verify_label_reqctx ctx(vl, vc);
    cobj_ref seg = COBJ(m.target.ep_segment->seg_ct,
			m.target.ep_segment->seg_id);
    if (!ctx.can_rw(seg)) {
	warn << "segment_exec2: cannot write to segment\n";
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    /*
     * Write message to segment
     */
    str s = xdr2str(m);
    uint64_t seglen = s.len() + 8;
    error_check(sys_segment_resize(seg, seglen));

    uint64_t *data_map = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &data_map, &seglen, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    memcpy(data_map + 1, s.cstr(), s.len());
    *data_map = 1;
    sys_sync_wakeup(data_map);

    da.cb(DELIVERY_DONE);
}

void
segment_exec(catmgr *cm, const dj_message &m, const delivery_args &da)
{
    try {
	segment_exec2(cm, m, da);
    } catch (std::exception &e) {
	warn << "segment_exec: " << e.what() << "\n";
	da.cb(DELIVERY_REMOTE_ERR);
    }

    cm->drop_now();
}
