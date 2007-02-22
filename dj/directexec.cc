#include <dj/directexec.hh>

void
dj_direct_gatemap::deliver(const dj_pubkey &sender,
			   const dj_message &a,
			   const delivery_args &da)
{
    if (a.target.type != EP_GATE) {
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    dj_msg_sink *s = gatemap_[COBJ(a.target.gate->gate_ct,
				   a.target.gate->gate_id)];
    if (!s) {
	da.cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    uint64_t token = ++counter_;
    (*s)(sender, a, token);
    da.cb(DELIVERY_DONE, token);
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

void
dj_direct_gatemap::destroy(const dj_message_endpoint &ep)
{
    if (ep.type != EP_GATE) {
	warn << "dj_direct_gatemap::destroy: not a gate\n";
	return;
    }

    cobj_ref cid = COBJ(ep.gate->gate_ct, ep.gate->gate_id);
    gatemap_.remove(cid);
}
