extern "C" {
#include <inc/atomic.h>
#include <inc/intmacro.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
#include <inc/stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djrpcx.h>

enum { retry_delivery_msec = 5000 };
enum { reply_none, reply_copying, reply_done };

struct dj_rpc_reply_state {
    uint64_t base_procct;
    uint64_t callct;
    dj_pubkey server;
    atomic64_t reply;
    uint64_t mdone;
    dj_message m;

    cobj_ref privgate;
    label privlabel;
    label privclear;
};

/*
 * XXX
 *
 * Race condition: what if we deallocate the dj_rpc_reply_state
 * while a slow thread has already entered the return gate?  One
 * answer is to addref ourselves to the call container and then
 * call set_sched_parents to ensure the call container is still
 * live.  This requires two args to the gate service function,
 * a minor inconvenience..
 *
 * Other race conditions also abound.  How to GC all unwanted
 * reply threads?  We can't kill them because they have stack
 * allocations.
 */
static void
dj_rpc_reply_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_reply_state *s = (dj_rpc_reply_state *) arg;

    if (start_env->proc_container != s->base_procct) {
	cprintf("dj_rpc_reply_entry: tainted, refusing to proceed\n");
	return;
    }

    label vl, vc;
    thread_cur_verify(&vl, &vc);

    dj_outgoing_gate_msg m;
    djgate_incoming(gcd, vl, vc, &m, r);

    if (m.sender != s->server) {
	printf("dj_rpc_reply_entry: reply from unexpected node\n");
	return;
    }

    if (atomic_compare_exchange64(&s->reply,
	    reply_none, reply_copying) != reply_none) {
	printf("dj_rpc_reply_entry: duplicate reply, dropping\n");
	return;
    }

    if (m.m.glabel.ents.size() || m.m.gclear.ents.size()) {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);

	label vl, vc;
	thread_cur_verify(&vl, &vc);

	vl.merge(&tl, &s->privlabel, label::min, label::leq_starhi);
	vc.merge(&tc, &s->privclear, label::max, label::leq_starlo);

	int64_t id = sys_gate_create(start_env->proc_container, 0,
				     s->privlabel.to_ulabel(),
				     s->privclear.to_ulabel(),
				     0, "djrpc reply privs", 0);
	if (id < 0) {
	    printf("dj_rpc_reply_entry: cannot save privs!\n");
	    return;
	}

	s->privgate = COBJ(start_env->proc_container, id);
    }

    s->m = m.m;
    assert(atomic_compare_exchange64(&s->reply,
		reply_copying, reply_done) == reply_copying);
    sys_sync_wakeup(&s->reply.counter);
}

static dj_delivery_code
dj_rpc_call_gate(gate_sender *gs, const dj_pubkey &node, time_t timeout,
		 const dj_delegation_set &dset, const dj_catmap &cm,
		 const dj_message &m, const str &calldata, dj_message *reply,
		 label *grantlabel, label *return_ct_taint)
{
    label lcallct;

    if (return_ct_taint) {
	lcallct = *return_ct_taint;
    } else {
	thread_cur_label(&lcallct);
	lcallct.transform(label::star_to, lcallct.get_default());
    }

    int64_t call_ct = sys_container_alloc(start_env->proc_container,
					  lcallct.to_ulabel(),
					  "djcall reply container",
					  0, 10 * 1024 * 1024);
    error_check(call_ct);
    scope_guard<int, cobj_ref> unref(sys_obj_unref,
				     COBJ(start_env->proc_container, call_ct));

    dj_rpc_reply_state rs;
    rs.base_procct = start_env->proc_container;
    atomic_set(&rs.reply, reply_none);
    rs.server = node;
    rs.callct = call_ct;

    gatesrv_descriptor gd;
    gd.gate_container_ = call_ct;
    gd.name_ = "dj_rpc_reply_entry";
    gd.func_ = &dj_rpc_reply_entry;
    gd.arg_ = (void *) &rs;
    cobj_ref return_gate = gate_create(&gd);

    dj_call_msg callmsg;
    callmsg.return_ep.set_type(EP_GATE);
    callmsg.return_ep.ep_gate->msg_ct = call_ct;
    callmsg.return_ep.ep_gate->gate.gate_ct = call_ct;
    callmsg.return_ep.ep_gate->gate.gate_id = return_gate.object;
    callmsg.return_cm = cm;
    callmsg.return_ds = dset;
    callmsg.buf = calldata;

    dj_message m2 = m;
    m2.msg = xdr2str(callmsg);
    m2.want_ack = 0;

    uint64_t timeout_at_msec = sys_clock_msec() + timeout * 1000;
    for (;;) {
	uint64_t now_msec = sys_clock_msec();
	if (now_msec >= timeout_at_msec)
	    return DELIVERY_TIMEOUT;

	dj_delivery_code code = gs->send(node,
					 (timeout_at_msec - now_msec) / 1000,
					 dset, cm, m2, grantlabel);
	if (code != DELIVERY_DONE)
	    return code;

	if (atomic_read(&rs.reply) == reply_none)
	    sys_sync_wait(&rs.reply.counter, reply_none,
			  sys_clock_msec() + retry_delivery_msec);

	while (atomic_read(&rs.reply) == reply_copying)
	    sys_sync_wait(&rs.reply.counter, reply_copying, ~0UL);

	if (atomic_read(&rs.reply) == reply_done)
	    break;
    }

    if (rs.m.glabel.ents.size() || rs.m.gclear.ents.size()) {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);

	label nl, nc;
	tl.merge(&rs.privlabel, &nl, label::min, label::leq_starlo);
	tc.merge(&rs.privclear, &nc, label::max, label::leq_starlo);

	struct jos_jmp_buf jb;
	if (!jos_setjmp(&jb)) {
	    struct thread_entry te;
	    memset(&te, 0, sizeof(te));
	    error_check(sys_self_get_as(&te.te_as));
	    te.te_entry = (void *) &jos_longjmp;
	    te.te_arg[0] = (uintptr_t) &jb;
	    te.te_arg[1] = 1;

	    int r = sys_gate_enter(rs.privgate, nl.to_ulabel(),
					        nc.to_ulabel(), &te);
	    printf("djsrpcc: cannot regain privs: %s\n", e2s(r));
	    return DELIVERY_LOCAL_ERR;
	}
	thread_label_cache_update(&nl, &nc);
	sys_obj_unref(rs.privgate);
    }

    *reply = rs.m;
    return DELIVERY_DONE;
}

static dj_delivery_code
dj_rpc_call_seg(gate_sender *gs, const dj_pubkey &node, time_t timeout,
		const dj_delegation_set &dset, const dj_catmap &cm,
		const dj_message &m, const str &calldata, dj_message *reply,
		label *grantlabel, label *return_ct_taint)
{
    label lseg;

    if (return_ct_taint) {
	lseg = *return_ct_taint;
    } else {
	thread_cur_label(&lseg);
	lseg.transform(label::star_to, lseg.get_default());
    }

    int64_t seg_id = sys_segment_create(start_env->shared_container,
					8, lseg.to_ulabel(), "gate return seg");
    error_check(seg_id);
    cobj_ref reply_seg = COBJ(start_env->shared_container, seg_id);
    scope_guard<int, cobj_ref> unref(sys_obj_unref, reply_seg);

    error_check(sys_container_move_quota(start_env->shared_container,
					 seg_id, 10 * 1024 * 1024));

    dj_call_msg callmsg;
    callmsg.return_ep.set_type(EP_SEGMENT);
    callmsg.return_ep.ep_segment->seg_ct = reply_seg.container;
    callmsg.return_ep.ep_segment->seg_id = reply_seg.object;
    callmsg.return_cm = cm;
    callmsg.return_ds = dset;
    callmsg.buf = calldata;

    dj_message m2 = m;
    m2.msg = xdr2str(callmsg);
    m2.want_ack = 0;

    uint64_t timeout_at_msec = sys_clock_msec() + timeout * 1000;
    for (;;) {
	uint64_t now_msec = sys_clock_msec();
	if (now_msec >= timeout_at_msec)
	    return DELIVERY_TIMEOUT;

	dj_delivery_code code = gs->send(node,
					 (timeout_at_msec - now_msec) / 1000,
					 dset, cm, m2, grantlabel);
	if (code != DELIVERY_DONE)
	    return code;

	uint64_t *base = 0;
	error_check(segment_map(reply_seg, 0, SEGMAP_READ,
				(void **) &base, 0, 0));
	scope_guard2<int, void*, int> unmap(segment_unmap_delayed, base, 1);

	if (*base == 0)
	    sys_sync_wait(base, 0, sys_clock_msec() + retry_delivery_msec);

	if (*base)
	    break;
    }

    uint64_t *base = 0;
    uint64_t len = 0;
    error_check(segment_map(reply_seg, 0, SEGMAP_READ,
			    (void **) &base, &len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, base, 1);

    base += 1;
    len -= 8;

    buf2xdr(*reply, (const void *) base, len);
    return DELIVERY_DONE;
}

dj_delivery_code
dj_rpc_call(gate_sender *gs, const dj_pubkey &node, time_t timeout,
	    const dj_delegation_set &dset, const dj_catmap &cm,
	    const dj_message &m, const str &calldata, dj_message *reply,
	    label *grantlabel, label *return_ct_taint, bool gateret)
{
    if (gateret)
	return dj_rpc_call_gate(gs, node, timeout, dset, cm, m, calldata, reply, grantlabel, return_ct_taint);
    else
	return dj_rpc_call_seg(gs, node, timeout, dset, cm, m, calldata, reply, grantlabel, return_ct_taint);
}
