#include <async.h>
#include <arpc.h>
#include <ihash.h>
#include <itree.h>
#include <sfscrypt.h>

#include <dj/bcast.hh>
#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djkey.hh>
#include <dj/cryptconn.hh>
#include <dj/perf.hh>

enum { conn_debug = 1 };
enum { bcast_debug = 0 };

enum {
    addr_cert_valid = 60,
    delegation_time_skew = 5,

    broadcast_period = 5,
    check_local_msgs = 0,
    direct_local_msgs = 1,
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

struct pk_addr {	/* d.a.addr speaks-for pk */
    ihash_entry<pk_addr> pk_link;
    itree_entry<pk_addr> exp_link;
    dj_timestamp expires;
    dj_pubkey pk;
    dj_delegation d;
};

struct msg_client {
    itree_entry<msg_client> link;
    dj_pubkey to;
    dj_stmt_signed ss;
    delivery_status_cb cb;
    void *local_deliver_arg;

    msg_client(const dj_pubkey &k) : to(k) {}
};

class djprot_impl : public djprot {
 public:
    djprot_impl(uint16_t port)
	: k_(sfscrypt.gen(SFS_RABIN, 0, SFS_SIGN | SFS_VERIFY |
					SFS_ENCRYPT | SFS_DECRYPT)),
	  exp_cb_(0)
    {
	xid_ = 0;
	bc_port_ = htons(port);

	int listenfd = inetsocket(SOCK_STREAM);
	if (listenfd < 0)
	    fatal << "djprot_impl: inetsocket\n";

	sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	if (getsockname(listenfd, (sockaddr *) &sin, &slen) < 0)
	    fatal << "djprot_impl: getsockname\n";
	my_port_ = sin.sin_port;

	int bfd = bcast_info.bind_bcast_sock(ntohs(bc_port_), true);
	in_addr curipaddr = myipaddr();
	warn << "djprot: listening on " << inet_ntoa(curipaddr)
	     << ":" << ntohs(my_port_) << ", broadcast port "
	     << ntohs(bc_port_) << "\n";
	warn << "djprot: my public key is " << pubkey() << "\n";

	listen(listenfd, 5);
	make_async(listenfd);
	fdcb(listenfd, selread, wrap(this, &djprot_impl::tcp_accept, listenfd));

	make_async(bfd);
	bx_ = axprt_dgram::alloc(bfd);
	bx_->setrcb(wrap(this, &djprot_impl::rcv_bcast));

	send_bcast();
    }

    virtual ~djprot_impl() {
	tcpconn_.deleteall();
	addr_exp_.deleteall();
	if (exp_cb_)
	    timecb_remove(exp_cb_);

	msg_client *ncc;
	for (msg_client *cc = clnt_.first(); cc; cc = ncc) {
	    ncc = clnt_.next(cc);
	    clnt_done(cc, DELIVERY_TIMEOUT);
	}

	warn << "djprot_impl dead\n";
    }

    virtual dj_pubkey pubkey() const {
	dj_pubkey dpk;
	assert(k_->export_pubkey(&dpk));
	return dpk;
    }

    virtual ptr<sfspriv> privkey() {
	return k_;
    }

    virtual void send(const dj_message &msg,
		      const dj_delegation_set &dset,
		      delivery_status_cb cb,
		      void *local_deliver_arg)
    {
	PERF_COUNTER(djprot::send);

	msg_client *cc = New msg_client(msg.to);
	clnt_.insert(cc);
	cc->cb = cb;
	cc->local_deliver_arg = local_deliver_arg;

	if (!labelcheck_send(msg, dset)) {
	    clnt_done(cc, DELIVERY_LOCAL_DELEGATION);
	    return;
	}

	cc->ss.stmt.set_type(STMT_MSG);
	*cc->ss.stmt.msg = msg;
	cc->ss.stmt.msg->from = pubkey();
	clnt_transmit(cc);
    }

    virtual void set_delivery_cb(local_delivery_cb cb) {
	local_delivery_ = cb;
    }

    virtual void sign_statement(dj_stmt_signed *s) {
	str buf = xdr2str(s->stmt);
	if (!buf)
	    fatal << "sign_statement: cannot encode\n";
	if (!k_->sign(&s->sign, buf))
	    fatal << "sign_statement: cannot sign\n";
    }

 private:
    bool labelcheck_send(const dj_message &a,
			 const dj_delegation_set &dset)
    {
	PERF_COUNTER(labelcheck_send);
	const dj_pubkey &dst = a.to;

	if (!check_local_msgs && dst == pubkey())
	    return true;

	dj_delegation_map dm(dset);

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    const dj_gcat &c = a.taint.ents[i];
	    if (c.integrity)
		continue;
	    if (!key_speaks_for(dst, c, dm, dm.size())) {
		warn << "labelcheck_send: missing delegation for taint "
		     << c << " for host " << dst << "\n";
		return false;
	    }
	}

	return true;
    }

    bool labelcheck_recv(const dj_message &a,
			 const dj_delegation_set &dset)
    {
	PERF_COUNTER(labelcheck_recv);
	const dj_pubkey &src = a.from;

	if (!check_local_msgs && src == pubkey())
	    return true;

	dj_delegation_map dm(dset);

	for (uint64_t i = 0; i < a.taint.ents.size(); i++) {
	    const dj_gcat &c = a.taint.ents[i];
	    if (!c.integrity)
		continue;
	    if (!key_speaks_for(src, c, dm, dm.size())) {
		warn << "labelcheck_recv: missing delegation for taint "
		     << c << " for host " << src << "\n";
		return false;
	    }
	}

	for (uint64_t i = 0; i < a.glabel.ents.size(); i++) {
	    const dj_gcat &c = a.glabel.ents[i];
	    if (!key_speaks_for(src, c, dm, dm.size())) {
		warn << "labelcheck_recv: missing delegation for grant "
		     << c << " for host " << src << "\n";
		return false;
	    }
	}

	for (uint64_t i = 0; i < a.gclear.ents.size(); i++) {
	    const dj_gcat &c = a.gclear.ents[i];
	    if (!key_speaks_for(src, c, dm, dm.size())) {
		warn << "labelcheck_recv: missing delegation for clear "
		     << c << " for host " << src << "\n";
		return false;
	    }
	}

	return true;
    }

    void clnt_done(msg_client *cc, dj_delivery_code code) {
	if (code != DELIVERY_DONE)
	    warn << "clnt_done: code " << code << "\n";
	if (cc->cb)
	    cc->cb(code);
	clnt_.remove(cc);
	delete cc;
    }

    void clnt_transmit(msg_client *cc) {
	PERF_COUNTER(clnt_transmit);

	if (cc->ss.stmt.msg->to == pubkey() && direct_local_msgs) {
	    process_msg(*cc->ss.stmt.msg, cc->local_deliver_arg);
	    clnt_done(cc, DELIVERY_DONE);
	    return;
	}

	crypt_conn *cxn = tcpconn_[cc->ss.stmt.msg->to];
	if (cxn) {
	    str msg = xdr2str(cc->ss.stmt);
	    if (!msg) {
		warn << "clnt_transmit: cannot encode statement\n";
		clnt_done(cc, DELIVERY_LOCAL_ERR);
		return;
	    }

	    cxn->send(msg);
	    clnt_done(cc, DELIVERY_DONE);
	    return;
	}

	pk_addr *a = addr_key_[cc->ss.stmt.msg->to];
	if (!a) {
	    warn << "send_message: can't find address for pubkey "
		 << cc->ss.stmt.msg->to << "\n";
	    clnt_done(cc, DELIVERY_NO_ADDRESS);
	    return;
	}
	dj_address addr = *a->d.a.addr;

	time_t now = time(0);
	if (now < a->d.from_ts - delegation_time_skew ||
	    now > a->d.until_ts + delegation_time_skew)
	{
	    warn << "send_message: expired address delegation\n";
	    clnt_done(cc, DELIVERY_NO_ADDRESS);
	    return;
	}

	cc->cb(DELIVERY_DONE);
	cc->cb = 0;

	tcp_connect(cc->ss.stmt.msg->to, addr);
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
	PERF_COUNTER(process_delegation);

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

    void srvr_send_no_status(dj_delivery_code code) {
    }

    void process_msg(const dj_message &m, void *local_deliver_arg) {
	PERF_COUNTER(process_msg);

	if (m.to != pubkey()) {
	    warn << "misrouted message to " << m.to << "\n";
	    return;
	}

	if (!local_delivery_) {
	    warn << "process_msg: missing delivery backend\n";
	    return;
	}

	if (!labelcheck_recv(m, m.dset)) {
	    warn << "process_msg: bad delegations\n";
	    return;
	}

	delivery_args da;
	da.cb = wrap(this, &djprot_impl::srvr_send_no_status);
	da.local_delivery_arg = local_deliver_arg;
	local_delivery_(m, da);
    }

    void rcv_bcast(const char *pkt, ssize_t len, const sockaddr *addr) {
	if (!pkt) {
	    warn << "receive error -- but it's UDP?\n";
	    return;
	}

	dj_stmt_signed m;
	if (!buf2xdr(m, pkt, len)) {
	    warn << "cannot decode incoming message\n";
	    return;
	}

	if (!verify_stmt(m)) {
	    warn << "Bad signature on statement\n";
	    return;
	}

	switch (m.stmt.type) {
	case STMT_DELEGATION:
	    if (bcast_debug)
		warn << "received bcast delegation..\n";

	    process_delegation(*m.stmt.delegation);
	    break;

	default:
	    warn << "Unhandled bcast statement type " << m.stmt.type << "\n";
	}
    }

    void send_bcast(void) {
	in_addr curipaddr = myipaddr();

	dj_stmt_signed s;
	s.stmt.set_type(STMT_DELEGATION);
	s.stmt.delegation->a.set_type(ENT_ADDRESS);
	s.stmt.delegation->a.addr->ip = curipaddr.s_addr;
	s.stmt.delegation->a.addr->port = my_port_;

	s.stmt.delegation->b.set_type(ENT_PUBKEY);
	*s.stmt.delegation->b.key = pubkey();

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

	    if (bcast_debug)
		warn << "Sent broadcast delegation to "
		     << inet_ntoa(*ap) << "\n";
	}

	// Chew on our own delegation, for good measure..
	process_delegation(*s.stmt.delegation);

	delaycb(broadcast_period, wrap(this, &djprot_impl::send_bcast));
    }

    void tcp_accept(int fd) {
	sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	int c = accept(fd, (sockaddr *) &sin, &sinlen);
	if (c < 0) {
	    warn << "accept: " << strerror(errno) << "\n";
	    return;
	}

	if (conn_debug)
	    warn << "djprot: accepted new connection\n";

	vNew crypt_conn(c, this,
			wrap(this, &djprot_impl::tcp_receive),
			wrap(this, &djprot_impl::tcp_ready));
    }

    void tcp_connect(const dj_pubkey &pk, const dj_address &addr) {
	in_addr inaddr;
	inaddr.s_addr = addr.ip;
	if (conn_debug)
	    warn << "djprot: opening connection to "
		 << inet_ntoa(inaddr) << ":" << ntohs(addr.port) << "\n";
	tcpconnect(inaddr, ntohs(addr.port),
		   wrap(this, &djprot_impl::tcp_connected, pk));
    }

    void tcp_connected(dj_pubkey pk, int fd) {
	if (fd < 0) {
	    warn << "tcp_connected: " << strerror(errno) << "\n";
	    return;
	}

	vNew crypt_conn(fd, pk, this,
			wrap(this, &djprot_impl::tcp_receive),
			wrap(this, &djprot_impl::tcp_ready));
    }

    void tcp_ready(crypt_conn *cc, crypt_conn_status code) {
	if (code == crypt_cannot_connect) {
	    warn << "cryptconn: cannot connect\n";

	    msg_client *nmc;
	    for (msg_client *mc = clnt_[cc->remote_];
		 mc && mc->to == cc->remote_; mc = nmc)
	    {
		nmc = clnt_.next(mc);
		clnt_done(mc, DELIVERY_TIMEOUT);
	    }

	    delete cc;
	    return;
	}

	if (code == crypt_disconnected) {
	    warn << "cryptconn: disconnected\n";
	    tcpconn_.remove(cc);
	    delete cc;
	    return;
	}

	if (code == crypt_connected) {
	    tcpconn_.insert(cc);

	    msg_client *nmc;
	    for (msg_client *mc = clnt_.first(); mc; mc = nmc) {
		nmc = clnt_.next(mc);
		clnt_transmit(mc);
	    }

	    return;
	}

	panic << "funny tcp_ready code " << code << "\n";
    }

    void tcp_receive(const dj_pubkey &sender, const str &msg) {
	dj_stmt stmt;
	if (!str2xdr(stmt, msg)) {
	    warn << "tcp_receive: cannot decode incoming message\n";
	    return;
	}

	if (stmt.type != STMT_MSG) {
	    warn << "tcp_receive: unexpected statement type\n";
	    return;
	}

	if (stmt.msg->from != sender) {
	    warn << "tcp_receive: sender vs envelope mismatch\n";
	    return;
	}

	process_msg(*stmt.msg, 0);
    }

    uint16_t bc_port_;	/* network byte order */
    uint16_t my_port_;	/* network byte order */
    ptr<axprt> bx_;
    ptr<sfspriv> k_;
    uint64_t xid_;

    ihash<dj_pubkey, pk_addr, &pk_addr::pk, &pk_addr::pk_link> addr_key_;
    itree<dj_timestamp, pk_addr, &pk_addr::expires, &pk_addr::exp_link> addr_exp_;
    itree<dj_pubkey, msg_client, &msg_client::to, &msg_client::link> clnt_;

    time_t exp_first_;
    timecb_t *exp_cb_;
    local_delivery_cb local_delivery_;

    ihash<dj_pubkey, crypt_conn, &crypt_conn::remote_, &crypt_conn::link_> tcpconn_;
};

djprot *
djprot::alloc(uint16_t port)
{
    return New djprot_impl(port);
}
