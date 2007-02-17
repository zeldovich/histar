#include <dj/directexec.hh>

void
dj_direct_gatemap::deliver(const dj_message_endpoint &ep,
			   const dj_message_args &a,
			   djprot::delivery_status_cb cb)
{
    if (ep.type != EP_GATE) {
	cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    dj_msg_sink *s = gatemap_[COBJ(ep.gate->gate_ct, ep.gate->gate_id)];
    if (!s) {
	cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    uint64_t token = ++counter_;
    cb(DELIVERY_DONE, token);
    (*s)(a, token);
}

dj_message_endpoint
dj_direct_gatemap::create_gate(uint64_t ct, dj_msg_sink cb)
{
    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.gate->gate_ct = ct;
    ep.gate->gate_id = ++counter_;
    gatemap_.insert(COBJ(ep.gate->gate_ct, ep.gate->gate_id), cb);
    return ep;
}
