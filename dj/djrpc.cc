#include <dj/dis.hh>
#include <dj/djops.hh>
#include <dj/djcall.h>

void
dj_rpc_call_sink(message_sender *s, dj_call_service srv,
		 const dj_message_args &a, uint64_t selftoken)
{
    dj_call_msg cm;
    if (!str2xdr(cm, a.msg)) {
	warn << "cannot unmarshal dj_call_msg\n";
	return;
    }

    dj_message_args reply;
    reply.msg_ct = cm.return_ct;
    reply.token = selftoken;

    srv(a, str(cm.buf.base(), cm.buf.size()), &reply);

    s->send(a.sender, cm.return_ep, reply, 0);
}

void
dj_rpc_call::call(const dj_esign_pubkey &node, const dj_message_endpoint &ep,
		  dj_message_args &a, const str &buf, call_reply_cb cb)
{
    a_ = a;
    dst_ = node;
    cb_ = cb;
    rep_ = f_->create_gate(rct_, wrap(this, &dj_rpc_call::reply_sink));
    rep_created_ = true;

    dj_call_msg cm;
    cm.return_ct = rct_;
    cm.return_ep = rep_;
    cm.buf = buf;

    a_.msg = xdr2str(cm);
    a_.token = 0;
    s_->send(node, ep, a_, wrap(this, &dj_rpc_call::delivery_cb));
}

void
dj_rpc_call::delivery_cb(dj_delivery_code c, uint64_t token)
{
    if (c != DELIVERY_DONE) {
	cb_(c, (const dj_message_args *) 0);
	return;
    }

    reply_token_ = token;

    /*
     * XXX
     * Server does not retransmit replies, so we should retry
     * if we don't hear back.
     */
}

void
dj_rpc_call::reply_sink(const dj_message_args &a, uint64_t token)
{
    if (reply_token_ == 0)
	return;		/* XXX wait for delivery_cb to set it... */

    if (a.sender != dst_) {
	warn << "dj_rpc_call::reply_sink: reply from wrong node\n";
	return;
    }

    if (reply_token_ != a.token) {
	warn << "dj_rpc_call::reply_sink: wrong reply token\n";
	return;
    }

    cb_(DELIVERY_DONE, &a);
}

dj_rpc_call::~dj_rpc_call()
{
    if (rep_created_)
	f_->destroy(rep_);
}
