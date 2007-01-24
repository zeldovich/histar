#include <async.h>
#include <arpc.h>
#include <dis.hh>
#include "dj.h"

class djserv_impl : public djserv {
 public:
    djserv_impl(uint16_t port) {
	int fd = inetsocket(SOCK_DGRAM, port, 0);
	if (fd < 0)
	    fatal << "inetsocket: " << strerror(errno) << "\n";

	make_async(fd);
	x_ = axprt_dgram::alloc(fd);
	x_->setrcb(wrap(mkref(this), &djserv_impl::rcv));
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

    ptr<axprt> x_;
};

ptr<djserv>
djserv::alloc(uint16_t port)
{
    ptr<djserv_impl> i = New refcounted<djserv_impl>(port);
    return i;
}
