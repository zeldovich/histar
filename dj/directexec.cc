#include <dj/directexec.hh>

void
dj_direct_gatemap::deliver(const dj_pubkey &sender,
			   const dj_message &a,
			   const delivery_args &da)
{
    if (a.target.type != EP_GATE) {
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    dj_msg_sink *s = gatemap_[COBJ(a.target.ep_gate->gate.gate_ct,
				   a.target.ep_gate->gate.gate_id)];
    if (!s) {
	da.cb(DELIVERY_REMOTE_ERR);
	return;
    }

    (*s)(sender, a);
    da.cb(DELIVERY_DONE);
}

dj_message_endpoint
dj_direct_gatemap::create_gate(uint64_t ct, dj_msg_sink cb)
{
    dj_message_endpoint ep;
    ep.set_type(EP_GATE);
    ep.ep_gate->msg_ct = 0xdeadbeef;
    ep.ep_gate->gate.gate_ct = ct;
    ep.ep_gate->gate.gate_id = ++counter_;
    gatemap_.insert(COBJ(ep.ep_gate->gate.gate_ct, ep.ep_gate->gate.gate_id), cb);
    return ep;
}

void
dj_direct_gatemap::destroy(const dj_message_endpoint &ep)
{
    if (ep.type != EP_GATE) {
	warn << "dj_direct_gatemap::destroy: not a gate\n";
	return;
    }

    cobj_ref cid = COBJ(ep.ep_gate->gate.gate_ct, ep.ep_gate->gate.gate_id);
    gatemap_.remove(cid);
}
