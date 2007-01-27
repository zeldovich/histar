#include <async.h>
#include <crypt.h>
#include <wmstr.h>
#include <arpc.h>
#include <esign.h>
#include <itree.h>

#include <dj/dis.hh>
#include <dj/bcast.hh>
#include <dj/dj.h>
#include <dj/djops.hh>

enum {
    keybits = 1024,
    broadcast_period = 5,
    addr_cert_valid = 60,
    time_skew = 5,
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
verify_stmt(const dj_stmt_signed &ss)
{
    switch (ss.stmt.type) {
    case STMT_DELEGATION:
	switch (ss.stmt.delegation->b.type) {
	case ENT_PUBKEY:
	    return verify_sign(ss.stmt, *ss.stmt.delegation->b.key, ss.sign);

	case ENT_GCAT:
	    return verify_sign(ss.stmt, ss.stmt.delegation->b.gcat->key, ss.sign);

	case ENT_ADDRESS:
	    printf("verify_stmt: cannot speak for a network address\n");
	    return false;

	default:
	    return false;
	}

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

class djprot_impl : public djprot {
 public:
    djprot_impl(uint16_t port)
	: k_(esign_keygen(keybits)), net_label_(1), net_clear_(1)
    {
	myport_ = htons(port);
	myipaddr_ = myipaddr();

	int fd = bcast_info.bind_bcast_sock(ntohs(myport_), true);
	warn << "djprot: listening on " << inet_ntoa(myipaddr_)
	     << ":" << ntohs(myport_) << "\n";
	warn << "djprot: my public key is {" << k_.n << "," << k_.k << "}\n";

	make_async(fd);
	x_ = axprt_dgram::alloc(fd);
	x_->setrcb(wrap(mkref(this), &djprot_impl::rcv));

	cache_cleanup();
	send_bcast();
    }
    virtual ~djprot_impl() { warn << "djprot_impl dead\n"; }

    virtual void set_label(const label &l) { net_label_ = l; }
    virtual void set_clear(const label &l) { net_clear_ = l; }

 private:
    void cache_cleanup(void) {
	time_t now = time(0);

	pk_addr *na;
	for (pk_addr *a = addr_cache_.first(); a; a = na) {
	    na = addr_cache_.next(a);
	    if (a->d.until_sec < now) {
		warn << "Purging pk_addr cache entry\n";
		addr_cache_.remove(a);
		delete a;
	    }
	}

	pk_spk4 *ns;
	for (pk_spk4 *s = spk4_cache_.first(); s; s = ns) {
	    ns = spk4_cache_.next(s);
	    if (s->d.until_sec < now) {
		warn << "Purging pk_spk4 cache entry\n";
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

    void rcv(const char *pkt, ssize_t len, const sockaddr *addr) {
	if (!pkt) {
	    warn << "receive error -- but it's UDP?\n";
	    return;
	}

	str p(pkt, len);
	dj_wire_msg m;
	if (!str2xdr(m, p)) {
	    warn << "cannot decode incoming message\n";
	    return;
	}

	switch (m.type) {
	case DJ_BCAST_STMT:
	    if (!verify_stmt(*m.s)) {
		warn << "Bad signature on statement\n";
		return;
	    }

	    switch (m.s->stmt.type) {
	    case STMT_DELEGATION:
		warn << "delegation: " << m.s->stmt.delegation->a
		     << " speaks-for " << m.s->stmt.delegation->b << "\n";

		if (m.s->stmt.delegation->a.type == ENT_ADDRESS)
		    update_netaddr(*m.s->stmt.delegation);
		if (m.s->stmt.delegation->a.type == ENT_PUBKEY)
		    update_speaksfor(*m.s->stmt.delegation);
		break;

	    default:
		warn << "Unhandled statement type " << m.s->stmt.type << "\n";
	    }
	    break;

	default:
	    warn << "Unhandled packet type " << m.type << "\n";
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
	s.stmt.delegation->a.addr->port = myport_;

	s.stmt.delegation->b.set_type(ENT_PUBKEY);
	s.stmt.delegation->b.key->n = k_.n;
	s.stmt.delegation->b.key->k = k_.k;

	time_t now = time(0);
	s.stmt.delegation->from_sec = now - time_skew;
	s.stmt.delegation->until_sec = now + addr_cert_valid + time_skew;
	sign_statement(&s);

	dj_wire_msg m;
	m.set_type(DJ_BCAST_STMT);
	*m.s = s;

	str msg = xdr2str(m);
	if (!msg)
	    fatal << "send_bcast: !xdr2str(m)\n";

	bcast_info.init();
	for (const in_addr *ap = bcast_info.bcast_addrs.base();
	     ap < bcast_info.bcast_addrs.lim(); ap++) {
	    sockaddr_in bcast;
	    bcast.sin_family = AF_INET;
	    bcast.sin_addr = *ap;
	    bcast.sin_port = myport_;
	    x_->send(msg.cstr(), msg.len(), (sockaddr *) &bcast);
	    warn << "Sent out broadcast address delegation to "
		 << inet_ntoa(*ap) << "\n";
	}

	delaycb(broadcast_period,
		wrap(mkref(this), &djprot_impl::send_bcast));
    }

    in_addr myipaddr_;	/* network byte order */
    uint16_t myport_;	/* network byte order */
    ptr<axprt> x_;
    esign_priv k_;

    itree<dj_esign_pubkey, pk_addr, &pk_addr::pk, &pk_addr::link> addr_cache_;
    itree<dj_esign_pubkey, pk_spk4, &pk_spk4::pk, &pk_spk4::link> spk4_cache_;

    label net_label_, net_clear_;
};

ptr<djprot>
djprot::alloc(uint16_t port)
{
    random_init();
    ptr<djprot_impl> i = New refcounted<djprot_impl>(port);
    return i;
}
