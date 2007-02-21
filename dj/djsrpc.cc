#include <dj/djrpc.hh>

dj_delivery_code
dj_rpc_call::call(const dj_pubkey &node, time_t timeout,
		  const dj_delegation_set &dset, const dj_catmap &cm,
		  const dj_message &m, dj_message *reply)
{
    return DELIVERY_TIMEOUT;    
}

void
dj_rpc_srv(dj_rpc_service_fn *fn, gate_call_data *gcd, gatesrv_return *ret)
{
    :wq

    label vl, vc;
    thread_cur_verify(&vl, &vc);

    dj_outgoing_gate_msg m;
    djgate_incoming(gcd, vl, vc, &m, r);

    dj_rpc_reply *reply;
    reply.

    if (!dj_echo_service(m.m, str, &reply))
	return;
}
