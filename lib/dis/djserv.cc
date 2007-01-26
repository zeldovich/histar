#include <async.h>
#include <wmstr.h>
#include <arpc.h>
#include <esign.h>
#include <dis.hh>
#include <bcast.hh>
#include "dj.h"

enum {
    keybits = 1024,
    broadcast_period = 5,
    addr_cert_valid = 60,
    time_skew = 5,
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

class djserv_impl : public djserv {
 public:
    djserv_impl(uint16_t port) : k_(esign_keygen(keybits)) {
	myport_ = htons(port);
	myipaddr_ = myipaddr();

	int fd = bcast_info.bind_bcast_sock(ntohs(myport_), true);
	warn << "djserv: listening on " << inet_ntoa(myipaddr_)
	     << ":" << ntohs(myport_) << "\n";

	make_async(fd);
	x_ = axprt_dgram::alloc(fd);
	x_->setrcb(wrap(mkref(this), &djserv_impl::rcv));

	send_bcast();
    }
    ~djserv_impl() { warn << "djserv_impl dead\n"; }

 private:
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

	warn << "got a packet, type " << m.t.type << "..\n";
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

	str buf = xdr2str(s.stmt);
	if (!buf)
	    fatal << "send_bcast: !xdr2str(s.stmt)\n";

	s.sign = k_.sign(buf);
	str msg = xdr2str(s);
	if (!msg)
	    fatal << "send_bcast: !xdr2str(s)\n";

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
		wrap(mkref(this), &djserv_impl::send_bcast));
    }

    in_addr myipaddr_;	/* network byte order */
    uint16_t myport_;	/* network byte order */
    ptr<axprt> x_;
    esign_priv k_;
};

ptr<djserv>
djserv::alloc(uint16_t port)
{
    ptr<djserv_impl> i = New refcounted<djserv_impl>(port);
    return i;
}
