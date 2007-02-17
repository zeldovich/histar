#include <dj/directexec.hh>

void
dj_direct_gatemap::deliver(const dj_message_endpoint &ep,
			   const dj_message_args &a,
			   djprot::delivery_status_cb cb)
{
    if (ep.type != ENDPT_GATE) {
	cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    dj_msg_sink *s = gatemap_[COBJ(ep.gate->gate_ct, ep.gate->gate_id)];
    if (!s) {
	cb(DELIVERY_REMOTE_ERR, 0);
	return;
    }

    uint64_t token = ++token_;
    cb(DELIVERY_DONE, token);
    (*s)(a, token);
}
