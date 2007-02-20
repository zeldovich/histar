#include <dj/djarpc.hh>
#include <dj/djrpcx.h>

void
dj_arpc_srv_sink(message_sender *s, dj_arpc_service srv,
		 const dj_pubkey &sender, const dj_message &m,
		 uint64_t selftoken)
{
    dj_call_msg cm;
    if (!str2xdr(cm, str(m.msg.base(), m.msg.size()))) {
	warn << "cannot unmarshal dj_call_msg\n";
	return;
    }

    dj_arpc_reply r;
    r.sender = sender;
    r.tmo = 0;
    r.dset = m.dset;
    r.msg.target = cm.return_ep;
    r.msg.msg_ct = cm.return_ct;
    r.msg.token = selftoken;

    r.msg.taint = m.taint;
    r.msg.glabel.deflevel = 3;
    r.msg.gclear.deflevel = 0;

    if (srv(m, str(cm.buf.base(), cm.buf.size()), &r))
	s->send(r.sender, r.tmo, r.dset, r.msg, 0);
}

void
dj_arpc_call::call(const dj_pubkey &node, time_t tmo, const dj_delegation_set &dset,
		   const dj_message &a, const str &buf, call_reply_cb cb)
{
    dset_ = dset;
    a_ = a;
    dst_ = node;
    cb_ = cb;
    rep_ = f_->create_gate(rct_, wrap(mkref(this), &dj_arpc_call::reply_sink));
    rep_created_ = true;

    dj_call_msg cm;
    cm.return_ct = rct_;
    cm.return_ep = rep_;
    cm.buf = buf;

    a_.msg = xdr2str(cm);
    a_.token = 0;
    until_ = time(0) + tmo;
    retransmit();
}

void
dj_arpc_call::retransmit()
{
    if (done_)
	return;

    time_t now = time(0);
    if (now > until_)
	now = until_;

    s_->send(dst_, until_ - now, dset_, a_, wrap(mkref(this), &dj_arpc_call::delivery_cb));

    if (now >= until_ && !done_) {
	done_ = true;
	cb_(DELIVERY_TIMEOUT, (const dj_message*) 0);
	return;
    }
}

void
dj_arpc_call::delivery_cb(dj_delivery_code c, uint64_t token)
{
    if (done_)
	return;

    if (c != DELIVERY_DONE) {
	done_ = true;
	cb_(c, (const dj_message*) 0);
	return;
    }

    delaycb(1, wrap(mkref(this), &dj_arpc_call::retransmit));

    reply_token_ = token;
    while (delivery_waiters_.size())
	delivery_waiters_.pop_back()();
}

void
dj_arpc_call::reply_sink(const dj_pubkey &sender, const dj_message &a, uint64_t)
{
    if (done_)
	return;

    if (reply_token_ == 0) {
	delivery_waiters_.push_back(wrap(mkref(this), &dj_arpc_call::reply_sink2, sender, a));
	return;
    }

    reply_sink2(sender, a);
}

void
dj_arpc_call::reply_sink2(dj_pubkey sender, dj_message a)
{
    if (sender != dst_) {
	warn << "dj_arpc_call::reply_sink2: reply from wrong node\n";
	return;
    }

    if (reply_token_ != a.token) {
	warn << "dj_arpc_call::reply_sink: wrong reply token\n";
	return;
    }

    done_ = true;
    cb_(DELIVERY_DONE, &a);
}

dj_arpc_call::~dj_arpc_call()
{
    if (rep_created_)
	f_->destroy(rep_);
}

bool
dj_echo_service(const dj_message &m, const str &s, dj_arpc_reply *r)
{
    r->msg.msg = s;
    return true;
}
