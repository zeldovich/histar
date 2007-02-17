#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <ihash.h>
#include <itree.h>

#include <dj/dis.hh>
#include <dj/bcast.hh>
#include <dj/dj.h>
#include <dj/djops.hh>
#include <dj/catmap.hh>

enum {
    keybits = 1024,
    addr_cert_valid = 60,
    delegation_time_skew = 5,

    broadcast_period = 5,
    cache_cleanup_period = 15,

    nocheck_local_msgs = 0,
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
verify_sign(const T &xdrblob, const dj_esign_pubkey &pk, const bigint &sig)
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
    dj_esign_pubkey pk;
    dj_delegation d;
};

struct msg_client {
    ihash_entry<msg_client> link;
    dj_msg_id id;
    dj_stmt_signed ss;
    djprot::delivery_status_cb cb;

    uint32_t tmo;
    uint32_t until;
    timecb_t *timecb;

    msg_client(const dj_esign_pubkey &k, uint64_t xid)
	: id(k, xid), timecb(0), tmo(1) {}
};

class djprot_impl : public djprot {
 public:
    djprot_impl(uint16_t port)
	: k_(esign_keygen(keybits)), net_label_(1), net_clear_(1)
    {
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
	ux_->setrcb(wrap(mkref(this), &djprot_impl::rcv));

	make_async(bfd);
	bx_ = axprt_dgram::alloc(bfd);
	bx_->setrcb(wrap(mkref(this), &djprot_impl::rcv));

	cache_cleanup();
	send_bcast();
    }

    virtual ~djprot_impl() {
	addr_exp_.deleteall();

	msg_client *ncc;
	for (msg_client *cc = clnt_.first(); cc; cc = ncc) {
	    ncc = clnt_.next(cc);
	    clnt_done(cc);
	}

	warn << "djprot_impl dead\n";
    }

    virtual str pubkey() const { return xdr2str(esignpub2dj(k_)); }
    virtual void set_label(const label &l) { net_label_ = l; }
    virtual void set_clear(const label &l) { net_clear_ = l; }

    virtual void send(str node_pk, const dj_message_endpoint &endpt,
		      const dj_message_args &args, delivery_status_cb cb)
    {
	dj_esign_pubkey target;
	if (!str2xdr(target, node_pk)) {
	    warn << "call: cannot unmarshal node_pk\n";
	    cb(DELIVERY_LOCAL_ERR, 0);
	    return;
	}

	msg_client *cc = New msg_client(target, ++xid_);
	cc->cb = cb;
	cc->until = time(0) + args.send_timeout;

	cc->ss.stmt.set_type(STMT_MSG_XFER);
	cc->ss.stmt.msgx->from = esignpub2dj(k_);
	cc->ss.stmt.msgx->to = target;
	cc->ss.stmt.msgx->xid = cc->id.xid;
	cc->ss.stmt.msgx->u.set_op(MSG_REQUEST);
	cc->ss.stmt.msgx->u.req->target = endpt;
	msgarg_hton(args, &*cc->ss.stmt.msgx->u.req);

	clnt_.insert(cc);
	clnt_transmit(cc);
    }

    virtual void set_delivery_cb(local_delivery_cb cb) {
	local_delivery_ = cb;
    }

    virtual void set_catmgr(ptr<catmgr> cmgr) {
	cmgr_ = cmgr;
    }

    virtual ptr<catmgr> get_catmgr() {
	return cmgr_;
    }

 private:
    dj_gcat cat2gcat(uint64_t cat) {
	dj_gcat gcat;
	if (!cmap_.l2g(cat, &gcat)) {
	    gcat.key = esignpub2dj(k_);
	    gcat.id = cat;
	    cmap_.insert(cat, gcat);
	}

	return gcat;
    }

    uint64_t gcat2cat(dj_gcat gcat) {
	uint64_t cat;
	if (!cmap_.g2l(gcat, &cat)) {
	    cat = cmgr_->alloc();
	    cmap_.insert(cat, gcat);
	}

	return cat;
    }

    void label_hton(const label &hl, dj_label *nl) {
	const struct ulabel *ul = hl.to_ulabel_const();
	nl->deflevel = ul->ul_default;
	uint32_t hlsize = ul->ul_nent;
	for (uint64_t i = 0; i < ul->ul_nent; i++)
	    if (LB_LEVEL(ul->ul_ent[i]) == ul->ul_default)
		hlsize--;

	nl->ents.setsize(hlsize);
	for (uint64_t i = 0, j = 0; i < ul->ul_nent; i++) {
	    uint64_t ent = ul->ul_ent[i];
	    uint32_t lv = LB_LEVEL(ent);
	    if (lv == ul->ul_default)
		continue;

	    nl->ents[j].cat = cat2gcat(LB_HANDLE(ent));
	    nl->ents[j].level = lv;
	    j++;
	}
    }

    bool label_ntoh(const dj_label &nl, label *hl) {
	uint32_t lv = nl.deflevel;
	if (lv > LB_LEVEL_STAR) {
	    warn << "label_ntoh: bad level\n";
	    return false;
	}

	hl->reset(lv);
	for (uint64_t i = 0; i < nl.ents.size(); i++) {
	    lv = nl.ents[i].level;
	    if (lv > LB_LEVEL_STAR) {
		warn << "label_ntoh: bad level\n";
		return false;
	    }
	    hl->set(gcat2cat(nl.ents[i].cat), lv);
	}

	return true;
    }

    void msgarg_hton(const dj_message_args &h, dj_message *n) {
	n->msg = h.msg;
	n->msg_ct = h.msg_ct;
	n->halted = h.halted;

	n->namedcats.cats.setsize(h.namedcats.size());
	for (uint64_t i = 0; i < h.namedcats.size(); i++)
	    n->namedcats.cats[i] = cat2gcat(h.namedcats[i]);

	label_hton(h.taint,  &n->taint);
	label_hton(h.glabel, &n->glabel);
	label_hton(h.gclear, &n->gclear);
    }

    bool msgarg_ntoh(const dj_message &n, dj_message_args *h) {
	h->msg = str(n.msg.base(), n.msg.size());
	h->msg_ct = n.msg_ct;
	h->halted = n.halted;

	h->namedcats.setsize(n.namedcats.cats.size());
	for (uint64_t i = 0; i < n.namedcats.cats.size(); i++)
	    h->namedcats[i] = gcat2cat(n.namedcats.cats[i]);

	return label_ntoh(n.taint,  &h->taint)  &&
	       label_ntoh(n.glabel, &h->glabel) &&
	       label_ntoh(n.gclear, &h->gclear);
    }

    bool key_speaks_for(const dj_esign_pubkey &k, const dj_gcat &gcat) {
	if (gcat.key == k)
	    return true;

	/*
	 * XXX
	 * Check user-provided delegation chains.
	 */
	return false;
    }

    /*
     * Node_L(c) = { *, if Node speaks for c; 0 otherwise }
     * N_L = net_label_; N_C = net_clear_
     * M_L = a.taint; M_G = a.grant
     */

    bool labelcheck_send(const dj_message &a, const dj_esign_pubkey &dst) {
	if (nocheck_local_msgs && dst == esignpub2dj(k_))
	    return true;

	/* M_L \leq (Node_L^\histar \cup N_C) */
	if (a.taint.deflevel > net_clear_.get_default())
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat gcat = a.taint.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t lv = a.taint.ents[i].level;
	    if (lv <= net_clear_.get(lcat))
		continue;

	    if (!key_speaks_for(dst, gcat))
		return false;
	}

	return true;
    }

    bool labelcheck_recv(const dj_message &a, const dj_esign_pubkey &src) {
	if (nocheck_local_msgs && src == esignpub2dj(k_))
	    return true;

	/*
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_L
	 * M_L \leq (Node_L^\histar \cup N_C)
	 *
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_G [approximately]
	 */
	if (a.taint.deflevel < net_label_.get_default() ||
	    a.taint.deflevel > net_clear_.get_default() ||
	    a.glabel.deflevel != 3 || a.gclear.deflevel != 0)
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat gcat = a.taint.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t lv = a.taint.ents[i].level;
	    if (lv >= LB_LEVEL_STAR)
		return false;
	    if (net_label_.get(lcat) <= lv && net_clear_.get(lcat) >= lv)
		continue;
	    if (!key_speaks_for(src, gcat))
		return false;
	}

	for (uint64_t i = 0; i < a.glabel.ents.size(); i++) {
	    dj_gcat gcat = a.glabel.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t lv = a.glabel.ents[i].level;
	    if (lv != LB_LEVEL_STAR)
		return false;
	    if (net_label_.get(lcat) == LB_LEVEL_STAR)
		continue;
	    if (!key_speaks_for(src, gcat))
		return false;
	}

	for (uint64_t i = 0; i < a.gclear.ents.size(); i++) {
	    dj_gcat gcat = a.gclear.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t lv = a.gclear.ents[i].level;
	    if (lv >= LB_LEVEL_STAR)
		return false;
	    if (net_clear_.get(lcat) >= lv)
		continue;
	    if (!key_speaks_for(src, gcat))
		return false;
	}

	return true;
    }

    bool send_message(str msg, dj_esign_pubkey nodekey) {
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

    void clnt_done(msg_client *cc) {
	if (cc->timecb)
	    timecb_remove(cc->timecb);
	clnt_.remove(cc);
	delete cc;
    }

    void clnt_transmit(msg_client *cc) {
	djprot::delivery_status_cb cb = cc->cb;
	cc->timecb = 0;

	if (!labelcheck_send(*cc->ss.stmt.msgx->u.req,
			     cc->ss.stmt.msgx->to)) {
	    clnt_done(cc);
	    cb(DELIVERY_NO_DELEGATION, 0);
	    return;
	}

	if (cc->ss.stmt.msgx->to == esignpub2dj(k_)) {
	    dj_msg_id cid(cc->ss.stmt.msgx->from, cc->ss.stmt.msgx->xid);
	    process_msg_request(*cc->ss.stmt.msgx, cid);
	    return;
	}

	sign_statement(&cc->ss);
	str msg = xdr2str(cc->ss);
	if (!msg) {
	    warn << "clnt_transmit: cannot encode msg xfer statement\n";
	    clnt_done(cc);
	    cb(DELIVERY_LOCAL_ERR, 0);
	    return;
	}

	if (!send_message(msg, cc->ss.stmt.msgx->to)) {
	    clnt_done(cc);
	    cb(DELIVERY_NO_ADDRESS, 0);
	    return;
	}

	time_t now = time(0);
	if (now >= cc->until) {
	    clnt_done(cc);
	    cb(DELIVERY_TIMEOUT, 0);
	    return;
	}

	cc->tmo *= 2;
	cc->timecb =
	    delaycb(cc->tmo, wrap(mkref(this),
				  &djprot_impl::clnt_transmit, cc));
    }

    void addr_remove(pk_addr *a) {
	addr_key_.remove(a);
	addr_exp_.remove(a);
	delete a;
    }

    void cache_cleanup(void) {
	time_t now = time(0);

	pk_addr *a = addr_exp_.first();
	while (a && a->expires < (now - delegation_time_skew)) {
	    pk_addr *xa = a;
	    a = addr_exp_.next(a);
	    addr_remove(xa);
	}

	delaycb(cache_cleanup_period,
		wrap(mkref(this), &djprot_impl::cache_cleanup));
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
	}
    }

    void srvr_send_status(dj_msg_id cid, dj_delivery_code code, uint64_t halted) {
	dj_stmt_signed ss;
	ss.stmt.set_type(STMT_MSG_XFER);
	ss.stmt.msgx->from = esignpub2dj(k_);
	ss.stmt.msgx->to = cid.key;
	ss.stmt.msgx->xid = cid.xid;
	ss.stmt.msgx->u.set_op(MSG_STATUS);
	ss.stmt.msgx->u.stat->set_code(code);
	if (code == DELIVERY_DONE)
	    *ss.stmt.msgx->u.stat->thread = halted;

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

	if (!labelcheck_recv(*c.u.req, c.from)) {
	    srvr_send_status(cid, DELIVERY_NO_DELEGATION, 0);
	    return;
	}

	dj_message_args a;
	if (!msgarg_ntoh(*c.u.req, &a)) {
	    warn << "process_msg_request: cannot unmarshal message\n";
	    srvr_send_status(cid, DELIVERY_REMOTE_ERR, 0);
	    return;
	}

	local_delivery_(c.u.req->target, a,
			wrap(mkref(this), &djprot_impl::srvr_send_status, cid));
    }

    void process_msg_status(const dj_msg_xfer &c, const dj_msg_id &cid) {
	msg_client *cc = clnt_[cid];
	if (!cc) {
	    warn << "process_msg_status: unexpected call reply\n";
	    return;
	}

	dj_delivery_code code = c.u.stat->code;
	uint64_t halted = (code == DELIVERY_DONE) ? *c.u.stat->thread : 0;
	cc->cb(code, halted);
	clnt_done(cc);
    }

    void process_msg(const dj_msg_xfer &c) {
	if (c.to != esignpub2dj(k_)) {
	    warn << "misrouted call to " << c.to << "\n";
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
	    warn << "process_call: unhandled op " << c.u.op << "\n";
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

	delaycb(broadcast_period,
		wrap(mkref(this), &djprot_impl::send_bcast));
    }

    in_addr myipaddr_;	/* network byte order */
    uint16_t bc_port_;	/* network byte order */
    uint16_t my_port_;	/* network byte order */
    ptr<axprt> ux_;
    ptr<axprt> bx_;
    esign_priv k_;
    uint64_t xid_;

    ihash<dj_esign_pubkey, pk_addr, &pk_addr::pk, &pk_addr::pk_link> addr_key_;
    itree<dj_timestamp, pk_addr, &pk_addr::expires, &pk_addr::exp_link> addr_exp_;
    ihash<dj_msg_id, msg_client, &msg_client::id, &msg_client::link> clnt_;

    label net_label_, net_clear_;
    local_delivery_cb local_delivery_;
    ptr<catmgr> cmgr_;
    catmap cmap_;
};

ptr<djprot>
djprot::alloc(uint16_t port)
{
    random_init();
    ptr<djprot_impl> i = New refcounted<djprot_impl>(port);
    return i;
}
