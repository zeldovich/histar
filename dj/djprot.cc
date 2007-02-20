#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <ihash.h>
#include <itree.h>

#include <dj/bcast.hh>
#include <dj/djprot.hh>
#include <dj/djops.hh>

enum {
    keybits = 128,
    //keybits = 1024,
    addr_cert_valid = 60,
    delegation_time_skew = 5,

    broadcast_period = 5,
    check_local_msgs = 1,
};

static in_addr
myipaddr(void)
{
    vec<in_addr> addrs;
    if (!myipaddrs(&addrs))
	fatal << "myipaddr: cannot get list of addresses\n";
    for (uint32_t i = 0; i < addrs.size(); i++)
	if (addrs[i].s_addr != htonl(INADDR_LOOPBACK))
	    return addrs[i];
    fatal << "myipaddr: no usable addresses\n";
}

template<class T>
static bool
verify_sign(const T &xdrblob, const dj_pubkey &pk, const bigint &sig)
{
    str msg = xdr2str(xdrblob);
    if (!msg)
	return false;

    return esign_pub(pk.n, pk.k).verify(msg, sig);  
}

static bool
verify_stmt(const dj_stmt_signed &s)
{
    switch (s.stmt.type) {
    case STMT_DELEGATION:
	switch (s.stmt.delegation->b.type) {
	case ENT_PUBKEY:
	    return verify_sign(s.stmt, *s.stmt.delegation->b.key, s.sign);

	case ENT_GCAT:
	    return verify_sign(s.stmt, s.stmt.delegation->b.gcat->key, s.sign);

	case ENT_ADDRESS:
	    printf("verify_stmt: cannot speak for a network address\n");
	    return false;

	default:
	    return false;
	}

    case STMT_MSG_XFER:
	return verify_sign(s.stmt, s.stmt.msgx->from, s.sign);

    default:
	return false;
    }
}

struct pk_addr {	/* d.a.addr speaks-for pk */
    ihash_entry<pk_addr> pk_link;
    itree_entry<pk_addr> exp_link;
    dj_timestamp expires;
    dj_pubkey pk;
    dj_delegation d;
};

struct msg_client {
    ihash_entry<msg_client> link;
    dj_msg_id id;
    dj_stmt_signed ss;
    delivery_status_cb cb;

    uint32_t until;
    timecb_t *timecb;
    uint32_t tmo;

    msg_client(const dj_pubkey &k, uint64_t xid)
	: id(k, xid), timecb(0), tmo(1) {}
};

class djprot_impl : public djprot {
 public:
    djprot_impl(uint16_t port)
	: k_(esign_keygen(keybits)), exp_cb_(0)
    {
	net_label_.deflevel = 1;
	net_clear_.deflevel = 1;

	xid_ = 0;
	bc_port_ = htons(port);
	myipaddr_ = myipaddr();

	int ufd = inetsocket(SOCK_DGRAM);
	if (ufd < 0)
	    fatal << "djprot_impl: inetsocket\n";

	sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	if (getsockname(ufd, (sockaddr *) &sin, &slen) < 0)
	    fatal << "djprot_impl: getsockname\n";
	my_port_ = sin.sin_port;

	int bfd = bcast_info.bind_bcast_sock(ntohs(bc_port_), true);
	warn << "djprot: listening on " << inet_ntoa(myipaddr_)
	     << ":" << ntohs(my_port_) << ", broadcast port "
	     << ntohs(bc_port_) << "\n";
	warn << "djprot: my public key is {" << k_.n << "," << k_.k << "}\n";

	make_async(ufd);
	ux_ = axprt_dgram::alloc(ufd);
	ux_->setrcb(wrap(this, &djprot_impl::rcv));

	make_async(bfd);
	bx_ = axprt_dgram::alloc(bfd);
	bx_->setrcb(wrap(this, &djprot_impl::rcv));

	send_bcast();
    }

    virtual ~djprot_impl() {
	addr_exp_.deleteall();
	if (exp_cb_)
	    timecb_remove(exp_cb_);

	msg_client *ncc;
	for (msg_client *cc = clnt_.first(); cc; cc = ncc) {
	    ncc = clnt_.next(cc);
	    clnt_done(cc, DELIVERY_TIMEOUT, 0);
	}

	warn << "djprot_impl dead\n";
    }

    virtual dj_pubkey pubkey() const { return esignpub2dj(k_); }
    virtual void set_label(const dj_label &l) { net_label_ = l; }
    virtual void set_clear(const dj_label &l) { net_clear_ = l; }

    virtual void send(const dj_pubkey &target, time_t timeout,
		      const dj_delegation_set &dset,
		      const dj_message &msg, delivery_status_cb cb)
    {
	msg_client *cc = New msg_client(target, ++xid_);
	clnt_.insert(cc);
	cc->cb = cb;
	cc->until = time(0) + timeout;

	if (!labelcheck_send(msg, target, dset)) {
	    clnt_done(cc, DELIVERY_LOCAL_DELEGATION, 0);
	    return;
	}

	cc->ss.stmt.set_type(STMT_MSG_XFER);
	cc->ss.stmt.msgx->from = esignpub2dj(k_);
	cc->ss.stmt.msgx->to = target;
	cc->ss.stmt.msgx->xid = cc->id.xid;
	cc->ss.stmt.msgx->u.set_op(MSG_REQUEST);
	*cc->ss.stmt.msgx->u.req = msg;

	clnt_transmit(cc);
    }

    virtual void set_delivery_cb(local_delivery_cb cb) {
	local_delivery_ = cb;
    }

 private:
    bool key_speaks_for(const dj_pubkey &k, const dj_gcat &gcat) {
	if (gcat.key == k)
	    return true;

	/*
	 * XXX
	 * feed dj_message.dj_delegation_set in here and check using that.
	 */
	return false;
    }

    /*
     * Node_L(c) = { *, if Node speaks for c; 0 otherwise }
     * N_L = net_label_; N_C = net_clear_
     * M_L = a.taint; M_G = a.grant
     */

    bool labelcheck_send(const dj_message &a, const dj_pubkey &dst,
			 const dj_delegation_set &dset)
    {
	if (!check_local_msgs && dst == esignpub2dj(k_))
	    return true;

	/* M_L \leq (Node_L^\histar \cup N_C) */
	if (a.taint.deflevel > net_clear_.deflevel)
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat c = a.taint.ents[i].cat;
	    uint32_t lv = a.taint.ents[i].level;
	    if (lv <= net_clear_ % c)
		continue;

	    if (!key_speaks_for(dst, c))
		return false;
	}

	return true;
    }

    bool labelcheck_recv(const dj_message &a, const dj_pubkey &src,
			 const dj_delegation_set &dset)
    {
	if (!check_local_msgs && src == esignpub2dj(k_))
	    return true;

	/*
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_L
	 * M_L \leq (Node_L^\histar \cup N_C)
	 *
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_G [approximately]
	 */
	if (a.taint.deflevel < net_label_.deflevel ||
	    a.taint.deflevel > net_clear_.deflevel ||
	    a.glabel.deflevel != 3 || a.gclear.deflevel != 0)
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat c = a.taint.ents[i].cat;
	    uint32_t lv = a.taint.ents[i].level;
	    if (lv >= LB_LEVEL_STAR)
		return false;
	    if (net_label_ % c <= lv && lv <= net_clear_ % c)
		continue;
	    if (!key_speaks_for(src, c))
		return false;
	}

	for (uint64_t i = 0; i < a.glabel.ents.size(); i++) {
	    dj_gcat c = a.glabel.ents[i].cat;
	    uint32_t lv = a.glabel.ents[i].level;
	    if (lv != LB_LEVEL_STAR)
		return false;
	    if (net_label_ % c == LB_LEVEL_STAR)
		continue;
	    if (!key_speaks_for(src, c))
		return false;
	}

	for (uint64_t i = 0; i < a.gclear.ents.size(); i++) {
	    dj_gcat c = a.gclear.ents[i].cat;
	    uint32_t lv = a.gclear.ents[i].level;
	    if (lv >= LB_LEVEL_STAR)
		return false;
	    if (lv <= net_clear_ % c)
		continue;
	    if (!key_speaks_for(src, c))
		return false;
	}

	return true;
    }

    bool send_message(str msg, dj_pubkey nodekey) {
	pk_addr *a = addr_key_[nodekey];
	if (!a) {
	    warn << "send_message: can't find address for pubkey\n";
	    return false;
	}
	dj_address addr = *a->d.a.addr;

	time_t now = time(0);
	if (now < a->d.from_ts - delegation_time_skew ||
	    now > a->d.until_ts + delegation_time_skew) {
	    warn << "send_message: expired address delegation\n";
	    return false;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr.ip;
	sin.sin_port = addr.port;
	ux_->send(msg.cstr(), msg.len(), (sockaddr *) &sin);
	return true;
    }

    void clnt_done(msg_client *cc, dj_delivery_code code, uint64_t token) {
	if (cc->cb)
	    cc->cb(code, token);
	if (cc->timecb)
	    timecb_remove(cc->timecb);
	clnt_.remove(cc);
	delete cc;
    }

    void clnt_transmit(msg_client *cc) {
	if (cc->timecb && time(0) >= cc->until) {
	    /* Have to transmit at least once for a timeout.. */
	    cc->timecb = 0;
	    clnt_done(cc, DELIVERY_TIMEOUT, 0);
	    return;
	}

	cc->tmo *= 2;
	cc->timecb = delaycb(cc->tmo, wrap(this, &djprot_impl::clnt_transmit, cc));

	if (cc->ss.stmt.msgx->to == esignpub2dj(k_)) {
	    dj_msg_id cid(cc->ss.stmt.msgx->from, cc->ss.stmt.msgx->xid);
	    process_msg_request(*cc->ss.stmt.msgx, cid);
	} else {
	    sign_statement(&cc->ss);
	    str msg = xdr2str(cc->ss);
	    if (!msg) {
		warn << "clnt_transmit: cannot encode msg xfer statement\n";
		clnt_done(cc, DELIVERY_LOCAL_ERR, 0);
		return;
	    }

	    if (!send_message(msg, cc->ss.stmt.msgx->to)) {
		clnt_done(cc, DELIVERY_NO_ADDRESS, 0);
		return;
	    }
	}
    }

    void addr_remove(pk_addr *a) {
	addr_key_.remove(a);
	addr_exp_.remove(a);
	delete a;
    }

    void cache_cleanup(void) {
	exp_cb_ = 0;
	time_t now = time(0);

	pk_addr *a = addr_exp_.first();
	while (a && a->expires <= (now - delegation_time_skew)) {
	    pk_addr *xa = a;
	    a = addr_exp_.next(a);
	    addr_remove(xa);
	}

	if (a) {
	    exp_first_ = a->expires;
	    exp_cb_ = timecb(a->expires + delegation_time_skew,
			     wrap(this, &djprot_impl::cache_cleanup));
	}
    }

    void process_delegation(const dj_delegation &d) {
	//warn << "delegation: " << d.a << " speaks-for " << d.b << "\n";

	if (d.a.type == ENT_ADDRESS) {
	    pk_addr *old = addr_key_[*d.b.key];
	    if (old && old->d.until_ts < d.until_ts)
		addr_remove(old);

	    pk_addr *pka = New pk_addr();
	    pka->pk = *d.b.key;
	    pka->d = d;
	    pka->expires = d.until_ts;

	    addr_key_.insert(pka);
	    addr_exp_.insert(pka);

	    if (!exp_cb_ || pka->expires < exp_first_) {
		if (exp_cb_)
		    timecb_remove(exp_cb_);
		exp_cb_ = timecb(pka->expires + delegation_time_skew,
				 wrap(this, &djprot_impl::cache_cleanup));
	    }
	}
    }

    void srvr_send_status(dj_msg_id cid, dj_delivery_code code, uint64_t token) {
	dj_stmt_signed ss;
	ss.stmt.set_type(STMT_MSG_XFER);
	ss.stmt.msgx->from = esignpub2dj(k_);
	ss.stmt.msgx->to = cid.key;
	ss.stmt.msgx->xid = cid.xid;
	ss.stmt.msgx->u.set_op(MSG_STATUS);
	ss.stmt.msgx->u.stat->set_code(code);
	if (code == DELIVERY_DONE)
	    *ss.stmt.msgx->u.stat->token = token;

	if (ss.stmt.msgx->to == esignpub2dj(k_)) {
	    dj_msg_id cid(ss.stmt.msgx->from, ss.stmt.msgx->xid);
	    process_msg_status(*ss.stmt.msgx, cid);
	    return;
	}

	sign_statement(&ss);
	str msg = xdr2str(ss);
	if (!msg) {
	    warn << "srvr_send_status: cannot encode reply\n";
	    return;
	}

	send_message(msg, cid.key);
    }

    void process_msg_request(const dj_msg_xfer &c, const dj_msg_id &cid) {
	if (!local_delivery_) {
	    warn << "process_msg_request: missing delivery backend\n";
	    srvr_send_status(cid, DELIVERY_REMOTE_ERR, 0);
	    return;
	}

	if (!labelcheck_recv(*c.u.req, c.from, c.u.req->dset)) {
	    srvr_send_status(cid, DELIVERY_REMOTE_DELEGATION, 0);
	    return;
	}

	local_delivery_(c.from, *c.u.req,
			wrap(this, &djprot_impl::srvr_send_status, cid));
    }

    void process_msg_status(const dj_msg_xfer &c, const dj_msg_id &cid) {
	msg_client *cc = clnt_[cid];
	if (!cc) {
	    warn << "process_msg_status: unexpected delivery status\n";
	    return;
	}

	dj_delivery_code code = c.u.stat->code;
	uint64_t token = (code == DELIVERY_DONE) ? *c.u.stat->token : 0;
	clnt_done(cc, code, token);
    }

    void process_msg(const dj_msg_xfer &c) {
	if (c.to != esignpub2dj(k_)) {
	    warn << "misrouted message to " << c.to << "\n";
	    return;
	}

	dj_msg_id cid(c.from, c.xid);

	switch (c.u.op) {
	case MSG_REQUEST:
	    process_msg_request(c, cid);
	    break;

	case MSG_STATUS:
	    process_msg_status(c, cid);
	    break;

	default:
	    warn << "process_msg: unhandled op " << c.u.op << "\n";
	}
    }

    void rcv(const char *pkt, ssize_t len, const sockaddr *addr) {
	if (!pkt) {
	    warn << "receive error -- but it's UDP?\n";
	    return;
	}

	str p(pkt, len);
	dj_stmt_signed m;
	if (!str2xdr(m, p)) {
	    warn << "cannot decode incoming message\n";
	    return;
	}

	if (!verify_stmt(m)) {
	    warn << "Bad signature on statement\n";
	    return;
	}

	switch (m.stmt.type) {
	case STMT_DELEGATION:
	    process_delegation(*m.stmt.delegation);
	    break;

	case STMT_MSG_XFER:
	    process_msg(*m.stmt.msgx);
	    break;

	default:
	    warn << "Unhandled statement type " << m.stmt.type << "\n";
	}
    }

    void sign_statement(dj_stmt_signed *s) {
	str buf = xdr2str(s->stmt);
	if (!buf)
	    fatal << "sign_statement: cannot encode\n";
	s->sign = k_.sign(buf);
    }

    void send_bcast(void) {
	dj_stmt_signed s;
	s.stmt.set_type(STMT_DELEGATION);
	s.stmt.delegation->a.set_type(ENT_ADDRESS);
	s.stmt.delegation->a.addr->ip = myipaddr_.s_addr;
	s.stmt.delegation->a.addr->port = my_port_;

	s.stmt.delegation->b.set_type(ENT_PUBKEY);
	*s.stmt.delegation->b.key = esignpub2dj(k_);

	time_t now = time(0);
	s.stmt.delegation->from_ts = now;
	s.stmt.delegation->until_ts = now + addr_cert_valid;
	sign_statement(&s);

	str msg = xdr2str(s);
	if (!msg)
	    fatal << "send_bcast: cannot encode message\n";

	bcast_info.init();
	for (const in_addr *ap = bcast_info.bcast_addrs.base();
	     ap < bcast_info.bcast_addrs.lim(); ap++) {
	    sockaddr_in bcast;
	    bcast.sin_family = AF_INET;
	    bcast.sin_addr = *ap;
	    bcast.sin_port = bc_port_;
	    bx_->send(msg.cstr(), msg.len(), (sockaddr *) &bcast);
	    //warn << "Sent broadcast delegation to " << inet_ntoa(*ap) << "\n";
	}

	delaycb(broadcast_period, wrap(this, &djprot_impl::send_bcast));
    }

    in_addr myipaddr_;	/* network byte order */
    uint16_t bc_port_;	/* network byte order */
    uint16_t my_port_;	/* network byte order */
    ptr<axprt> ux_;
    ptr<axprt> bx_;
    esign_priv k_;
    uint64_t xid_;

    ihash<dj_pubkey, pk_addr, &pk_addr::pk, &pk_addr::pk_link> addr_key_;
    itree<dj_timestamp, pk_addr, &pk_addr::expires, &pk_addr::exp_link> addr_exp_;
    ihash<dj_msg_id, msg_client, &msg_client::id, &msg_client::link> clnt_;

    time_t exp_first_;
    timecb_t *exp_cb_;
    dj_label net_label_, net_clear_;
    local_delivery_cb local_delivery_;
};

djprot *
djprot::alloc(uint16_t port)
{
    random_init();
    return New djprot_impl(port);
}
