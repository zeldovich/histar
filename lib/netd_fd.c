#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/netdevent.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/bipipe.h>
#include <inc/labelutil.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>

static void
libc_to_netd(struct sockaddr_in *sin, struct netd_sockaddr_in *nsin)
{
    nsin->sin_addr = sin->sin_addr.s_addr;
    nsin->sin_port = sin->sin_port;
}

static void
netd_to_libc(struct netd_sockaddr_in *nsin, struct sockaddr_in *sin)
{
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = nsin->sin_addr;
    sin->sin_port = nsin->sin_port;
}

int
socket(int domain, int type, int protocol)
{
    struct cobj_ref netd_gate = netd_get_gate();

    struct Fd *fd;
    int r = fd_alloc(&fd, "socket fd");
    if (r < 0)
	return r;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, socket) + sizeof(a.socket);

    a.op_type = netd_op_socket;
    a.socket.domain = domain;
    a.socket.type = type;
    a.socket.protocol = protocol;
    int sock = netd_call(netd_gate, &a);

    if (sock < 0) {
	jos_fd_close(fd);
	return sock;
    }

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_sock.s = sock;
    fd->fd_sock.netd_gate = netd_gate;
    return fd2num(fd);
}

int
socketpair(int domain, int type, int protocol, int sv[2])
{
    // fudge the socketpair
    return bipipe(sv);
}

static int
sock_bind(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, bind) + sizeof(a.bind);

    struct sockaddr_in sin;
    if (addrlen < sizeof(sin))
	   return -E_INVAL;

    memcpy(&sin, addr, sizeof(sin));

    a.op_type = netd_op_bind;
    a.bind.fd = fd->fd_sock.s;
    libc_to_netd(&sin, &a.bind.sin);
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static int
sock_connect(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, connect) + sizeof(a.connect);

    struct sockaddr_in sin;
    if (addrlen < sizeof(sin))
	   return -E_INVAL;

    memcpy(&sin, addr, sizeof(sin));

    a.op_type = netd_op_connect;
    a.connect.fd = fd->fd_sock.s;
    libc_to_netd(&sin, &a.connect.sin);
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static int
sock_listen(struct Fd *fd, int backlog)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, listen) + sizeof(a.listen);

    a.op_type = netd_op_listen;
    a.listen.fd = fd->fd_sock.s;
    a.listen.backlog = backlog;
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static int
sock_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, accept) + sizeof(a.accept);

    struct sockaddr_in sin;
    if (*addrlen < sizeof(sin))
	   return -E_INVAL;

    struct Fd *nfd;
    int r = fd_alloc(&nfd, "socket fd -- accept");
    if (r < 0)
	return r;

    a.op_type = netd_op_accept;
    a.accept.fd = fd->fd_sock.s;
    int sock = netd_call(fd->fd_sock.netd_gate, &a);

    if (sock < 0) {
	jos_fd_close(nfd);
	return sock;
    }

    nfd->fd_dev_id = devsock.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_sock.s = sock;
    nfd->fd_sock.netd_gate = fd->fd_sock.netd_gate;

    netd_to_libc(&a.accept.sin, &sin);
    memcpy(addr, &sin, sizeof(sin));

    return fd2num(nfd);
}

static ssize_t
sock_send(struct Fd *fd, const void *buf, size_t count, int flags)
{
    if (count > netd_buf_size)
	count = netd_buf_size;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, send) +
	     offsetof(struct netd_op_send_args, buf) + count;

    a.op_type = netd_op_send;
    a.send.fd = fd->fd_sock.s;
    a.send.count = count;
    a.send.flags = flags;
    memcpy(&a.send.buf[0], buf, count);
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static ssize_t
sock_sendto(struct Fd *fd, const void *buf, size_t count, int flags,
	    const struct sockaddr *to, socklen_t tolen)
{
    if (count > netd_buf_size)
	count = netd_buf_size;

    struct sockaddr_in sin;
    if (tolen < sizeof(sin))
	return -E_INVAL;
    
    memcpy(&sin, to, sizeof(sin));

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, sendto) +
	     offsetof(struct netd_op_sendto_args, buf) + count;

    a.op_type = netd_op_sendto;
    a.sendto.fd = fd->fd_sock.s;
    a.sendto.count = count;
    a.sendto.flags = flags;
    libc_to_netd(&sin, &a.sendto.sin);
    memcpy(&a.sendto.buf[0], buf, count);
    
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static ssize_t
sock_recv(struct Fd *fd, void *buf, size_t count, int flags)
{
    if (count > netd_buf_size)
	count = netd_buf_size;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, recv) +
	     offsetof(struct netd_op_recv_args, buf);

    a.op_type = netd_op_recv;
    a.recv.fd = fd->fd_sock.s;
    a.recv.count = count;
    a.recv.flags = flags;
    int r = netd_call(fd->fd_sock.netd_gate, &a);
    if (r > 0)
	memcpy(buf, &a.recv.buf[0], r);
    return r;
}

static ssize_t
sock_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return sock_send(fd, buf, count, 0);
}

static ssize_t
sock_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return sock_recv(fd, buf, count, 0);
}

static int
sock_close(struct Fd *fd)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, close) + sizeof(a.close);
    
    a.op_type = netd_op_close;
    a.close.fd = fd->fd_sock.s;
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static int
sock_shutdown(struct Fd *fd, int how)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, shutdown) + sizeof(a.shutdown);

    a.op_type = netd_op_shutdown;
    a.shutdown.fd = fd->fd_sock.s;
    a.shutdown.how = how;
    return netd_call(fd->fd_sock.netd_gate, &a);
}

static int
sock_getsockname(struct Fd *fd, struct sockaddr *addr, 
                 socklen_t *addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, getsockname) + sizeof(a.getsockname);

    struct sockaddr_in sin;

    if (*addrlen < sizeof(sin))
	   return -E_INVAL;

    a.op_type = netd_op_getsockname;
    a.getsockname.fd = fd->fd_sock.s;
    int r = netd_call(fd->fd_sock.netd_gate, &a);
    netd_to_libc(&a.getsockname.sin, &sin);
    memcpy(addr, &sin, sizeof(sin));
    *addrlen = sizeof(sin);
    return r;
}

static int 
sock_getpeername(struct Fd *fd, struct sockaddr *addr, 
                 socklen_t *addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, getpeername) + sizeof(a.getpeername);
    struct sockaddr_in sin;

    if (*addrlen < sizeof(sin))
	   return -E_INVAL;

    a.op_type = netd_op_getpeername;
    a.getpeername.fd = fd->fd_sock.s;
    int r = netd_call(fd->fd_sock.netd_gate, &a);
    netd_to_libc(&a.getpeername.sin, &sin);
    memcpy(addr, &sin, sizeof(sin));
    return r;
}

static int
sock_setsockopt(struct Fd *fd, int level, int optname, 
                const void *optval, socklen_t optlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, setsockopt) + sizeof(a.setsockopt);

    if (optlen > sizeof(a.setsockopt.optval))
	return -E_INVAL;

    if (level == SOL_SOCKET) {
	// LWIP does not support these, so fake it..
	if (optname == SO_REUSEADDR || optname == SO_REUSEPORT)
	    return 0;
    }

    a.op_type = netd_op_setsockopt;
    a.setsockopt.level = level;
    a.setsockopt.optname = optname;
    memcpy(&a.setsockopt.optval[0], optval, optlen);
    a.setsockopt.optlen = optlen;
    a.setsockopt.fd = fd->fd_sock.s;
    return netd_call(fd->fd_sock.netd_gate, &a);
}
    
static int
sock_getsockopt(struct Fd *fd, int level, int optname,
                void *optval, socklen_t *optlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, getsockopt) + sizeof(a.getsockopt);

    a.op_type = netd_op_getsockopt;
    a.getsockopt.level = level;
    a.getsockopt.optname = optname;
    a.getsockopt.fd = fd->fd_sock.s;
    int r = netd_call(fd->fd_sock.netd_gate, &a);

    if (a.getsockopt.optlen > *optlen)
	return -E_INVAL;
    *optlen = a.getsockopt.optlen;
    memcpy(optval, &a.getsockopt.optval[0], a.getsockopt.optlen);
    return r;
}

static int
sock_stat(struct Fd *fd, struct stat *buf)
{
    buf->st_mode |= __S_IFSOCK;
    return 0;
}

static int
sock_probe(struct Fd *fd, dev_probe_t probe)
{
    return netd_probe(fd, probe);
}

static int
sock_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{    
    return netd_wstat(fd, probe, wstat);
}

static int
sock_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    switch (req) {
    case SIOCGIFCONF: {
	struct ifconf *ifc = va_arg(ap, struct ifconf *);
	if ((uint32_t)ifc->ifc_len < sizeof(struct ifreq)) {
	    errno = ENOBUFS;
	    return -1;
	}
	struct ifreq *r = (struct ifreq *)ifc->ifc_buf;
	netd_name(r->ifr_name);
	r->ifr_name[2] = 0;
	
	struct netd_sockaddr_in nsin;
	netd_ip(&nsin);
	netd_to_libc(&nsin, (struct sockaddr_in *)&r->ifr_addr);
	ifc->ifc_len = sizeof(struct ifreq);

	return 0;
    }
    case SIOCGIFFLAGS: {
	struct ifreq *r = va_arg(ap, struct ifreq *);
	netd_name(r->ifr_name);
	r->ifr_name[2] = 0;
	
	r->ifr_flags = 0;
	netd_flags(&r->ifr_flags);
	return 0;
    }
    default:
	break;
    }
    return -1;
}

struct Dev devsock = 
{
    .dev_id = 's',
    .dev_name = "sock",
    .dev_read = sock_read,
    .dev_write = sock_write,
    .dev_recv = sock_recv,
    .dev_send = sock_send,
    .dev_sendto = sock_sendto,
    .dev_close = sock_close,
    .dev_bind = sock_bind,
    .dev_connect = sock_connect,
    .dev_listen = sock_listen,
    .dev_accept = sock_accept,
    .dev_getsockname = sock_getsockname,
    .dev_getpeername = sock_getpeername,
    .dev_setsockopt = sock_setsockopt,
    .dev_getsockopt = sock_getsockopt,
    .dev_shutdown = sock_shutdown,
    .dev_stat = sock_stat,
    .dev_probe = sock_probe,
    .dev_statsync = sock_statsync,
    .dev_ioctl = sock_ioctl,
};
