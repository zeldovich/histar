extern "C" {
#include <inc/atomic.h>
#include <inc/intmacro.h>
#include <inc/syscall.h>
}

#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djrpcx.h>

enum { token_done = (UINT64(1)<<63) };
enum { retry_delivery_msec = 5000 };

struct dj_rpc_reply_state {
    dj_pubkey server;
    atomic64_t token;
    uint64_t mdone;
    dj_message m;
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
 *
 * Would be nice if we could accept an older reply after having
 * timed out on waiting for it and sent a new request..  Keep a
 * vector of acceptable reply tokens?  Watch out for taint_cow()
 * not having happened (otherwise not safe).
 */
static void
dj_rpc_reply_entry(void *arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_reply_state *s = (dj_rpc_reply_state *) arg;

    label vl, vc;
    thread_cur_verify(&vl, &vc);

    dj_outgoing_gate_msg m;
    djgate_incoming(gcd, vl, vc, &m, r);

    if (m.sender != s->server) {
	printf("dj_rpc_reply_entry: reply from unexpected node\n");
	return;
    }

    for (;;) {
	uint64_t curtok = atomic_read(&s->token);
	if (curtok == m.m.token) {
	    uint64_t newtok = m.m.token | token_done;
	    if (atomic_compare_exchange64(&s->token, curtok, newtok) == curtok) {
		sys_sync_wakeup(&s->token.counter);
		break;
	    }
	}

	sys_sync_wait(&s->token.counter, curtok, ~0UL);
    }

    s->m = m.m;
    s->mdone = 1;
    sys_sync_wakeup(&s->mdone);

    /*
     * XXX
     *
     * What if we got some privilege, either glabel or gclear?
     * Should create an unbound gate with a process_grant:0 verify,
     * and have the dj_rpc_call() code jump through it.
     */
}

dj_delivery_code
dj_rpc_call(gate_sender *gs, const dj_pubkey &node, time_t timeout,
	    const dj_delegation_set &dset, const dj_catmap &cm,
	    const dj_message &m, const str &calldata, dj_message *reply,
	    label *grantlabel)
{
    label lcallct(1);

    int64_t call_ct = sys_container_alloc(start_env->proc_container,
					  lcallct.to_ulabel(),
					  "djcall reply container",
					  0, 10 * 1024 * 1024);
    error_check(call_ct);
    scope_guard<int, cobj_ref> unref(sys_obj_unref,
				     COBJ(start_env->proc_container, call_ct));

    dj_rpc_reply_state rs;
    atomic_set(&rs.token, 0);
    rs.server = node;
    rs.mdone = 0;

    gatesrv_descriptor gd;
    gd.gate_container_ = call_ct;
    gd.name_ = "dj_rpc_reply_entry";
    gd.func_ = &dj_rpc_reply_entry;
    gd.arg_ = (void *) &rs;
    cobj_ref return_gate = gate_create(&gd);

    dj_call_msg callmsg;
    callmsg.return_ct = call_ct;
    callmsg.return_ep.set_type(EP_GATE);
    callmsg.return_ep.gate->gate_ct = call_ct;
    callmsg.return_ep.gate->gate_id = return_gate.object;
    callmsg.return_cm = cm;
    callmsg.return_ds = dset;
    callmsg.buf = calldata;

    dj_message m2 = m;
    m2.token = 0;
    m2.msg = xdr2str(callmsg);

    uint64_t timeout_at_msec = sys_clock_msec() + timeout * 1000;
    for (;;) {
	uint64_t now_msec = sys_clock_msec();
	if (now_msec >= timeout_at_msec)
	    return DELIVERY_TIMEOUT;

	uint64_t token;

	dj_delivery_code code = gs->send(node,
					 (timeout_at_msec - now_msec) / 1000,
					 dset, cm, m2, &token, grantlabel);
	if (code != DELIVERY_DONE)
	    return code;

	if (token & token_done) {
	    printf("dj_rpc_call: reserved bit in token already set\n");
	    return DELIVERY_LOCAL_ERR;
	}

	if (atomic_compare_exchange64(&rs.token, 0, token) != 0) {
	    printf("dj_rpc_call: token CAS failure\n");
	    return DELIVERY_LOCAL_ERR;
	}

	sys_sync_wakeup(&rs.token.counter);
	sys_sync_wait(&rs.token.counter, token,
		      sys_clock_msec() + retry_delivery_msec);
	uint64_t ntoken = atomic_compare_exchange64(&rs.token, token, 0);
	if (ntoken != token) {
	    if (ntoken != (token | token_done)) {
		printf("dj_rpc_call: funny new token value\n");
		return DELIVERY_LOCAL_ERR;
	    }

	    while (!rs.mdone)
		sys_sync_wait(&rs.mdone, 0, ~0UL);
	    *reply = rs.m;
	    return DELIVERY_DONE;
	}
    }
}
