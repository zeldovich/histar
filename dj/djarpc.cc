#include <dj/djarpc.hh>
#include <dj/djrpcx.h>

static void
dj_arpc_srv_cb(message_sender *s, bool ok, const dj_rpc_reply &r)
{
    if (ok)
	s->send(r.sender, r.tmo, r.dset, r.msg, 0, 0);
}

void
dj_arpc_srv_sink(message_sender *s, dj_arpc_service srv,
		 const dj_pubkey &sender, const dj_message &m)
{
    dj_call_msg cm;
    if (!bytes2xdr(cm, m.msg)) {
	warn << "cannot unmarshal dj_call_msg\n";
	return;
    }

    dj_arpc_reply r;
    r.r.sender = sender;
    r.r.tmo = 0;
    r.r.dset = m.dset;
    r.r.catmap = m.catmap;
    r.r.msg.target = cm.return_ep;

    r.r.msg.taint = m.taint;
    r.r.msg.catmap = cm.return_cm;
    r.r.msg.dset = cm.return_ds;

    r.cb = wrap(&dj_arpc_srv_cb, s);

    srv(m, str(cm.buf.base(), cm.buf.size()), r);
}

void
dj_rpc_to_arpc(dj_rpc_service_cb srv, const dj_message &m, const str &s,
	       const dj_arpc_reply &r)
{
    dj_rpc_reply rr = r.r;
    bool ok = srv(m, s, &rr);
    r.cb(ok, rr);
}

void
dj_arpc_call::call(const dj_pubkey &node, time_t tmo, const dj_delegation_set &dset,
		   const dj_message &a, const str &buf, call_reply_cb cb,
		   const dj_catmap *return_cm, const dj_delegation_set *return_ds)
{
    dset_ = dset;
    a_ = a;
    dst_ = node;
    cb_ = cb;
    rep_ = f_->create_gate(rct_, wrap(mkref(this), &dj_arpc_call::reply_sink));
    rep_created_ = true;

    dj_call_msg cm;
    cm.return_ep = rep_;
    if (return_cm)
	cm.return_cm = *return_cm;
    if (return_ds)
	cm.return_ds = *return_ds;
    cm.buf = buf;

    a_.msg = xdr2str(cm);
    a_.want_ack = 0;
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

    s_->send(dst_, until_ - now, dset_, a_,
	     wrap(mkref(this), &dj_arpc_call::delivery_cb), 0);

    if (now >= until_ && !done_) {
	done_ = true;
	cb_(DELIVERY_TIMEOUT, (const dj_message*) 0);
	return;
    }
}

void
dj_arpc_call::delivery_cb(dj_delivery_code c)
{
    if (done_)
	return;

    if (c != DELIVERY_DONE) {
	done_ = true;
	cb_(c, (const dj_message*) 0);
	return;
    }

    delaycb(1, wrap(mkref(this), &dj_arpc_call::retransmit));
}

void
dj_arpc_call::reply_sink(const dj_pubkey &sender, const dj_message &a)
{
    if (done_)
	return;

    if (sender != dst_) {
	warn << "dj_arpc_call::reply_sink: reply from wrong node\n";
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
