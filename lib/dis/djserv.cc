#include <async.h>
#include <wmstr.h>
#include <arpc.h>
#include <esign.h>
#include <dis.hh>
#include "dj.h"

enum {
    keybits = 1024,
    broadcast_period = 5,
    addr_cert_valid = 60,
    time_skew = 5,
};

class djserv_impl : public djserv {
 public:
    djserv_impl(uint16_t port) : k_(esign_keygen(keybits)) {
	int fd = inetsocket(SOCK_DGRAM, port, 0);
	if (fd < 0)
	    fatal << "inetsocket: " << strerror(errno) << "\n";

	socklen_t len = sizeof(myaddr_);
	if (getsockname(fd, (sockaddr *) &myaddr_, &len) < 0)
	    fatal << "getsockname: " << strerror(errno) << "\n";

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
	s.stmt.delegation->a.addr->ip = ntohl(myaddr_.sin_addr.s_addr);
	s.stmt.delegation->a.addr->port = ntohs(myaddr_.sin_port);

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

	sockaddr_in bcast;
	bcast.sin_family = AF_INET;
	bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	bcast.sin_port = myaddr_.sin_port;
	x_->send(msg.cstr(), msg.len(), (sockaddr *) &bcast);
	warn << "Sent out broadcast address delegation\n";

	delaycb(broadcast_period,
		wrap(mkref(this), &djserv_impl::send_bcast));
    }

    sockaddr_in myaddr_;
    ptr<axprt> x_;
    esign_priv k_;
};

ptr<djserv>
djserv::alloc(uint16_t port)
{
    ptr<djserv_impl> i = New refcounted<djserv_impl>(port);
    return i;
}
