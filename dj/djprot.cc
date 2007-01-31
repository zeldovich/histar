#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <itree.h>

#include <dj/dis.hh>
#include <dj/bcast.hh>
#include <dj/dj.h>
#include <dj/djops.hh>

enum {
    keybits = 1024,
    addr_cert_valid = 60,
    delegation_time_skew = 5,
    call_timestamp_skew = 60,

    broadcast_period = 5,
    cache_cleanup_period = 10,
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
    dj_reply_status stat;
    djcall_args reply;
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
    virtual ~djprot_impl() { warn << "djprot_impl dead\n"; }

    virtual str pubkey() const { return xdr2str(esignpub2dj(k_)); }
    virtual void set_label(const label &l) { net_label_ = l; }
    virtual void set_clear(const label &l) { net_clear_ = l; }

    virtual void call(str node_pk, const dj_gatename &gate,
		      const djcall_args &args, call_reply_cb cb) {
	djcall_args reparg;
	reparg.taint = net_label_;
	reparg.grant = label(3);

	dj_esign_pubkey target;
	if (!str2xdr(target, node_pk)) {
	    warn << "call: cannot unmarshal node_pk\n";
	    cb(REPLY_SYSERR, reparg);
	    return;
	}

	/* Check if a taint of (l) can be sent to nodepk.  */

	/* Check if any categories in (g) are non-globalized, and if so,
	 * create new global categories for them. */

	/* Check the expiration of the address delegation?
	 * Check expiration of other delegations?
	 * Do we set call timeout value here, or is the server responsible
	 * for expiring the call when the delegations expire?  The latter
	 * would allow a call to be prolonged by additional delegations,
	 * but would require the server to repeat this delegation search
	 * process.
	 */

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
	cc->ss.stmt.call->u.req->arg.buf.setsize(args.data.len());
	memcpy(cc->ss.stmt.call->u.req->arg.buf.base(),
	       args.data.cstr(), args.data.len());
	//cc->ss.stmt.call->u.req->arg.label = XXX;
	//cc->ss.stmt.call->u.req->arg.grant = XXX;

	clnt_.insert(cc);
	clnt_transmit(cc);
    }

    virtual void set_callexec(callexec_factory cb) {
	execcb_ = cb;
    }

 private:
    void clnt_done(call_client *cc) {
	if (cc->timecb)
	    timecb_remove(cc->timecb);
	clnt_.remove(cc);
	delete cc;
    }

    void clnt_transmit(call_client *cc) {
	djprot::call_reply_cb cb = cc->cb;
	djcall_args reparg;
	reparg.taint = net_label_;
	reparg.grant = label(3);

	cc->timecb = 0;
	cc->ss.stmt.call->seq++;
	cc->ss.stmt.call->ts = time(0);
	sign_statement(&cc->ss);

	str msg = xdr2str(cc->ss);
	if (!msg) {
	    warn << "call: cannot encode call statement\n";
	    clnt_done(cc);
	    cb(REPLY_SYSERR, reparg);
	    return;
	}

	pk_addr *a = addr_cache_[cc->ss.stmt.call->to];
	if (!a || a->pk != cc->ss.stmt.call->to) {
	    warn << "call: can't find address for pubkey\n";
	    clnt_done(cc);
	    cb(REPLY_ADDRESS_MISSING, reparg);
	    return;
	}
	dj_address addr = *a->d.a.addr;

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr.ip;
	sin.sin_port = addr.port;
	ux_->send(msg.cstr(), msg.len(), (sockaddr *) &sin);

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

    void execcb(dj_reply_status stat, const djcall_args &a) {
	warn << "execcb: call completed, status " << stat << "\n";
    }

    void process_call(const dj_call &c) {
	if (c.to != esignpub2dj(k_)) {
	    warn << "misrouted call to " << c.to << "\n";
	    return;
	}

	switch (c.u.op) {
	case CALL_REQUEST:
	    if (execcb_) {
		djcall_args a;
		a.data = str(c.u.req->arg.buf.base(), c.u.req->arg.buf.size());

		/* Translate c.u.req->label, c.u.req->grant */

		ptr<djcallexec> e =
		    execcb_(wrap(mkref(this), &djprot_impl::execcb));
		e->start(c.u.req->gate, a);
	    } else {
		/* XXX reply with an error? */
	    }
	    break;

	case CALL_REPLY:
	    warn << "call reply: " << c.u.reply->stat << "\n";
	    break;

	case CALL_INPROGRESS:
	    warn << "call in progress reply\n";
	    break;

	case CALL_ABORT:
	    warn << "call abort\n";
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
};

ptr<djprot>
djprot::alloc(uint16_t port)
{
    random_init();
    ptr<djprot_impl> i = New refcounted<djprot_impl>(port);
    return i;
}
