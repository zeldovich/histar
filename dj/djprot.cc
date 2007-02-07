#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <itree.h>

#include <dj/dis.hh>
#include <dj/bcast.hh>
#include <dj/dj.h>
#include <dj/djops.hh>
#include <dj/catmap.hh>

/*
 * XXX
 *
 * Expirations of non-address delegations are not used for
 * category garbage-collection or call timeouts.
 */

enum {
    keybits = 1024,
    addr_cert_valid = 60,
    delegation_time_skew = 5,
    call_timestamp_skew = 60,

    broadcast_period = 5,
    cache_cleanup_period = 15,
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

    case STMT_CALL:
	return verify_sign(s.stmt, s.stmt.call->from, s.sign);

    default:
	return false;
    }
}

struct pk_addr {	/* d.a.addr speaks-for pk */
    itree_entry<pk_addr> link;
    dj_esign_pubkey pk;
    dj_delegation d;
};

struct pk_spk4 {	/* pk speaks for d.b */
    itree_entry<pk_spk4> link;
    dj_esign_pubkey pk;
    dj_delegation d;
};

struct call_client {
    itree_entry<call_client> link;
    djcall_id id;
    dj_stmt_signed ss;
    djprot::call_reply_cb cb;
    uint32_t tmo;
    timecb_t *timecb;
};

struct call_server {
    itree_entry<call_server> link;
    djcall_id id;
    ptr<djcallexec> exec;
    dj_reply_status stat;
    djcall_args reply;
    uint64_t replyseq;
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
	addr_cache_.deleteall();
	spk4_cache_.deleteall();
	srvr_.deleteall();

	call_client *ncc;
	for (call_client *cc = clnt_.first(); cc; cc = ncc) {
	    ncc = clnt_.next(cc);
	    clnt_done(cc);
	}

	warn << "djprot_impl dead\n";
    }

    virtual str pubkey() const { return xdr2str(esignpub2dj(k_)); }
    virtual void set_label(const label &l) { net_label_ = l; }
    virtual void set_clear(const label &l) { net_clear_ = l; }

    virtual void call(str node_pk, const dj_gatename &gate,
		      const djcall_args &args, call_reply_cb cb) {
	dj_esign_pubkey target;
	if (!str2xdr(target, node_pk)) {
	    warn << "call: cannot unmarshal node_pk\n";
	    cb(REPLY_SYSERR, (const djcall_args *) 0);
	    return;
	}

	call_client *cc = New call_client();
	cc->id.xid = ++xid_;
	cc->id.key = target;
	cc->cb = cb;
	cc->tmo = 1;

	cc->ss.stmt.set_type(STMT_CALL);
	cc->ss.stmt.call->xid = cc->id.xid;
	cc->ss.stmt.call->seq = 0;
	cc->ss.stmt.call->from = esignpub2dj(k_);
	cc->ss.stmt.call->to = target;
	cc->ss.stmt.call->u.set_op(CALL_REQUEST);
	cc->ss.stmt.call->u.req->gate = gate;
	cc->ss.stmt.call->u.req->timeout_sec = 0xffffffff;
	if (!callarg_hton(args, &cc->ss.stmt.call->u.req->arg)) {
	    cb(REPLY_SYSERR, (const djcall_args *) 0);
	    return;
	}

	clnt_.insert(cc);
	clnt_transmit(cc);
    }

    virtual void set_callexec(callexec_factory cb) {
	execcb_ = cb;
    }

    virtual void set_catmgr(ptr<catmgr> cmgr) {
	cmgr_ = cmgr;
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

    bool callarg_hton(const djcall_args &h, dj_gate_arg *n) {
	n->buf.setsize(h.data.len());
	memcpy(n->buf.base(), h.data.cstr(), h.data.len());

	if (h.grant.get_default() != 3) {
	    warn << "call: non-conforming grant label (bad default)\n";
	    return false;
	}

	const ulabel *ul = h.grant.to_ulabel_const();
	n->grant.setsize(ul->ul_nent);
	for (uint64_t i = 0; i < ul->ul_nent; i++) {
	    if (LB_LEVEL(ul->ul_ent[i]) != LB_LEVEL_STAR) {
		warn << "call: non-conforming grant label (bad level)\n";
		return false;
	    }

	    n->grant[i] = cat2gcat(LB_HANDLE(ul->ul_ent[i]));
	}

	ul = h.taint.to_ulabel_const();
	n->taint.deflevel = ul->ul_default;
	n->taint.ents.setsize(ul->ul_nent);
	for (uint64_t i = 0; i < ul->ul_nent; i++) {
	    n->taint.ents[i].cat = cat2gcat(LB_HANDLE(ul->ul_ent[i]));
	    n->taint.ents[i].level = LB_LEVEL(ul->ul_ent[i]);
	    if (n->taint.ents[i].level == LB_LEVEL_STAR) {
		warn << "call: star level in message taint\n";
		return false;
	    }
	}

	return true;
    }

    bool callarg_ntoh(const dj_gate_arg &n, djcall_args *h) {
	h->data = str(n.buf.base(), n.buf.size());

	if (n.taint.deflevel >= LB_LEVEL_STAR) {
	    warn << "callarg_ntoh: bad default level\n";
	    return false;
	}
	h->taint.reset(n.taint.deflevel);
	h->grant.reset(3);

	for (uint64_t i = 0; i < n.taint.ents.size(); i++) {
	    if (n.taint.ents[i].level >= LB_LEVEL_STAR) {
		warn << "callarg_ntoh: bad level\n";
		return false;
	    }
	    h->taint.set(gcat2cat(n.taint.ents[i].cat), n.taint.ents[i].level);
	}

	for (uint64_t i = 0; i < n.grant.size(); i++)
	    h->grant.set(gcat2cat(n.grant[i]), LB_LEVEL_STAR);

	return true;
    }

    bool key_speaks_for(const dj_esign_pubkey &k, const dj_gcat &gcat) {
	pk_spk4 *s = spk4_cache_[k];
	while (s && s->pk == k) {
	    if (s->d.b.type == ENT_GCAT && *s->d.b.gcat == gcat)
		return true;
	    if (s->d.b.type == ENT_PUBKEY)
		warn << "key_speaks_for: no recursion yet..\n";
	    s = spk4_cache_.next(s);
	}
	return false;
    }

    /*
     * Node_L(c) = { *, if Node speaks for c; 0 otherwise }
     * N_L = net_label_; N_C = net_clear_
     * M_L = a.taint; M_G = a.grant
     */

    bool labelcheck_send(const dj_gate_arg &a, const dj_esign_pubkey &k) {
	/* M_L \leq (Node_L^\histar \cup N_C) */
	if (a.taint.deflevel > net_clear_.get_default())
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat gcat = a.taint.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t level = a.taint.ents[i].level;
	    if (level >= LB_LEVEL_STAR)
		return false;

	    if (level <= net_clear_.get(lcat))
		continue;

	    if (!key_speaks_for(k, gcat))
		return false;
	}

	return true;
    }

    bool labelcheck_recv(const dj_gate_arg &a, const dj_esign_pubkey &k) {
	/*
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_L
	 * M_L \leq (Node_L^\histar \cup N_C)
	 *
	 * (Node_L^\histar \cup N_L^\histar)^\star \leq M_G [approximately]
	 */
	if (a.taint.deflevel < net_label_.get_default() ||
	    a.taint.deflevel > net_clear_.get_default())
	    return false;

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    dj_gcat gcat = a.taint.ents[i].cat;
	    uint64_t lcat = gcat2cat(gcat);
	    uint32_t level = a.taint.ents[i].level;
	    if (level >= LB_LEVEL_STAR)
		return false;

	    if (net_label_.get(lcat) <= level && net_clear_.get(lcat) >= level)
		continue;

	    if (!key_speaks_for(k, gcat))
		return false;
	}

	for (uint64_t i = 0; i < a.grant.size(); i++) {
	    dj_gcat gcat = a.grant[i];
	    uint64_t lcat = gcat2cat(gcat);
	    if (net_label_.get(lcat) == LB_LEVEL_STAR)
		continue;
	    if (!key_speaks_for(k, gcat))
		return false;
	}

	return true;
    }

    bool send_message(str msg, dj_esign_pubkey nodekey) {
	pk_addr *a = addr_cache_[nodekey];
	if (!a || a->pk != nodekey) {
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

    void clnt_done(call_client *cc) {
	if (cc->timecb)
	    timecb_remove(cc->timecb);
	clnt_.remove(cc);
	delete cc;
    }

    void clnt_transmit(call_client *cc) {
	djprot::call_reply_cb cb = cc->cb;

	cc->timecb = 0;
	cc->ss.stmt.call->seq++;
	cc->ss.stmt.call->ts = time(0);
	sign_statement(&cc->ss);

	if (!labelcheck_send(cc->ss.stmt.call->u.req->arg,
			     cc->ss.stmt.call->to)) {
	    clnt_done(cc);
	    cb(REPLY_DELEGATION_MISSING, (const djcall_args *) 0);
	    return;
	}

	str msg = xdr2str(cc->ss);
	if (!msg) {
	    warn << "call: cannot encode call statement\n";
	    clnt_done(cc);
	    cb(REPLY_SYSERR, (const djcall_args *) 0);
	    return;
	}

	if (!send_message(msg, cc->ss.stmt.call->to)) {
	    clnt_done(cc);
	    cb(REPLY_ADDRESS_MISSING, (const djcall_args *) 0);
	    return;
	}

	cc->tmo *= 2;
	cc->timecb =
	    delaycb(cc->tmo, wrap(mkref(this),
				  &djprot_impl::clnt_transmit, cc));
    }

    void cache_cleanup(void) {
	time_t now = time(0);

	pk_addr *na;
	for (pk_addr *a = addr_cache_.first(); a; a = na) {
	    na = addr_cache_.next(a);
	    if (a->d.until_ts < now) {
		//warn << "Purging pk_addr cache entry\n";
		addr_cache_.remove(a);
		delete a;
	    }
	}

	pk_spk4 *ns;
	for (pk_spk4 *s = spk4_cache_.first(); s; s = ns) {
	    ns = spk4_cache_.next(s);
	    if (s->d.until_ts < now) {
		//warn << "Purging pk_spk4 cache entry\n";
		spk4_cache_.remove(s);
		delete s;
	    }
	}

	delaycb(cache_cleanup_period,
		wrap(mkref(this), &djprot_impl::cache_cleanup));
    }

    void update_netaddr(const dj_delegation &d) {
	pk_addr *pka = New pk_addr();
	pka->pk = *d.b.key;
	pka->d = d;
	addr_cache_.insert(pka);
    }

    void update_speaksfor(const dj_delegation &d) {
	pk_spk4 *pks = New pk_spk4();
	pks->pk = *d.a.key;
	pks->d = d;
	spk4_cache_.insert(pks);
    }

    void process_delegation(const dj_delegation &d) {
	//warn << "delegation: " << d.a << " speaks-for " << d.b << "\n";

	if (d.a.type == ENT_ADDRESS)
	    update_netaddr(d);
	if (d.a.type == ENT_PUBKEY)
	    update_speaksfor(d);
    }

    void srvr_send_reply(call_server *cs) {
	dj_stmt_signed ss;
	ss.stmt.set_type(STMT_CALL);
	ss.stmt.call->xid = cs->id.xid;
	ss.stmt.call->seq = ++cs->replyseq;
	ss.stmt.call->ts = time(0);
	ss.stmt.call->from = esignpub2dj(k_);
	ss.stmt.call->to = cs->id.key;
	ss.stmt.call->u.set_op(CALL_REPLY);
	ss.stmt.call->u.reply->set_stat(cs->stat);
	if (cs->stat == REPLY_DONE) {
	    if (!callarg_hton(cs->reply, &*ss.stmt.call->u.reply->arg))
		return;
	    if (!labelcheck_send(*ss.stmt.call->u.reply->arg, cs->id.key))
		return;
	}
	sign_statement(&ss);

	str msg = xdr2str(ss);
	if (!msg) {
	    warn << "srvr_send_reply: cannot encode reply\n";
	    return;
	}

	send_message(msg, cs->id.key);
    }

    void execcb(call_server *cs, dj_reply_status stat, const djcall_args *a) {
	cs->stat = stat;
	if (stat == REPLY_DONE)
	    cs->reply = *a;
	cs->exec = 0;
	srvr_send_reply(cs);
    }

    void process_call(const dj_call &c) {
	if (c.to != esignpub2dj(k_)) {
	    warn << "misrouted call to " << c.to << "\n";
	    return;
	}

	/* XXX check c.ts */

	djcall_id cid;
	cid.key = c.from;
	cid.xid = c.xid;

	switch (c.u.op) {
	case CALL_REQUEST: {
	    call_server *cs = srvr_[cid];
	    if (cs && cs->id == cid) {
		srvr_send_reply(cs);
		return;
	    }

	    if (execcb_) {
		cs = New call_server();
		cs->id = cid;
		cs->stat = REPLY_INPROGRESS;
		cs->reply.taint = net_label_;
		cs->reply.grant = label(3);
		cs->replyseq = 0;
		cs->exec = 
		    execcb_(wrap(mkref(this), &djprot_impl::execcb, cs));
		srvr_.insert(cs);

		if (!labelcheck_recv(c.u.req->arg, c.from)) {
		    cs->stat = REPLY_DELEGATION_MISSING;
		    srvr_send_reply(cs);
		    return;
		}

		djcall_args a;
		callarg_ntoh(c.u.req->arg, &a);
		cs->exec->start(c.u.req->gate, a);
	    } else {
		warn << "process_call: missing execution backend\n";
	    }
	    break;
	}

	case CALL_ABORT: {
	    call_server *cs = srvr_[cid];
	    if (!cs || cs->id != cid) {
		warn << "unexpected call abort\n";
		return;
	    }

	    if (cs->exec)
		cs->exec->abort();
	    else
		srvr_send_reply(cs);
	    break;
	}

	case CALL_REPLY: {
	    call_client *cc = clnt_[cid];
	    if (!cc || cc->id != cid) {
		warn << "unexpected call reply\n";
		return;
	    }

	    if (c.u.reply->stat == REPLY_INPROGRESS) {
		warn << "call reply: in progress..\n";
		return;
	    }

	    if (c.u.reply->stat == REPLY_DONE) {
		djcall_args reparg;
		reparg.taint = net_label_;
		reparg.grant = label(3);

		if (!labelcheck_recv(*c.u.reply->arg, c.from))
		    return;
		if (!callarg_ntoh(*c.u.reply->arg, &reparg))
		    return;
		cc->cb(c.u.reply->stat, &reparg);
	    } else {
		cc->cb(c.u.reply->stat, (const djcall_args *) 0);
	    }
	    clnt_done(cc);
	    break;
	}

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

	case STMT_CALL:
	    process_call(*m.stmt.call);
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

    itree<dj_esign_pubkey, pk_addr, &pk_addr::pk, &pk_addr::link> addr_cache_;
    itree<dj_esign_pubkey, pk_spk4, &pk_spk4::pk, &pk_spk4::link> spk4_cache_;

    itree<djcall_id, call_client, &call_client::id, &call_client::link> clnt_;
    itree<djcall_id, call_server, &call_server::id, &call_server::link> srvr_;

    label net_label_, net_clear_;
    callexec_factory execcb_;
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
