#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/sockfd.h>
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
#include <stdio.h>

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
netd_socket(int domain, int type, int protocol)
{
    struct cobj_ref netd_gate = netd_get_gate();

    struct Fd *fd;
    int r = fd_alloc(&fd, "socket fd");
    if (r < 0)
	return r;
    /* netd_call relies on this being set */
    fd->fd_sock.netd_gate = netd_gate;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, socket) + sizeof(a.socket);

    a.op_type = netd_op_socket;
    a.socket.domain = domain;
    a.socket.type = type;
    a.socket.protocol = protocol;
    int sock = netd_call(fd, &a);

    if (sock < 0) {
	jos_fd_close(fd);
	return sock;
    }

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_sock.s = sock;
    return fd2num(fd);
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
    return netd_call(fd, &a);
}

static int
sock_connect(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, connect) + sizeof(a.connect);

    struct sockaddr_in sin;
    if (addrlen < sizeof(sin))
	   return -E_INVAL;

    a.op_type = netd_op_connect;
    a.connect.fd = fd->fd_sock.s;
    libc_to_netd((struct sockaddr_in *) addr, &a.connect.sin);
    return netd_call(fd, &a);
}

static int
sock_listen(struct Fd *fd, int backlog)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, listen) + sizeof(a.listen);

    a.op_type = netd_op_listen;
    a.listen.fd = fd->fd_sock.s;
    a.listen.backlog = backlog;
    return netd_call(fd, &a);
}

static int
sock_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, accept) + sizeof(a.accept);

    struct sockaddr_in sin;
    if (addrlen && *addrlen < sizeof(sin))
	return -E_INVAL;

    struct Fd *nfd;
    int r = fd_alloc(&nfd, "socket fd -- accept");
    if (r < 0)
	return r;

    a.op_type = netd_op_accept;
    a.accept.fd = fd->fd_sock.s;
    int sock = netd_call(fd, &a);

    if (sock < 0) {
	jos_fd_close(nfd);
	return sock;
    }

    nfd->fd_dev_id = devsock.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_sock.s = sock;
    nfd->fd_sock.netd_gate = fd->fd_sock.netd_gate;

    netd_to_libc(&a.accept.sin, &sin);
    if (addr)
	memcpy(addr, &sin, sizeof(sin));

    return fd2num(nfd);
}

static ssize_t
sock_sendmsg(struct Fd *fd, const struct msghdr *msg, int flags)
{
    if (msg->msg_control) {
	errno = EOPNOTSUPP;
	return -1;
    }

    uint32_t iovbytes = 0;
    for (uint32_t i = 0; i < msg->msg_iovlen; i++)
	iovbytes += msg->msg_iov[i].iov_len;

    if (iovbytes > netd_buf_size) {
	errno = EMSGSIZE;
	return -1;
    }

    struct netd_op_args a;
    struct sockaddr_in sin;
    uint32_t cc = 0;
    if (msg->msg_name) {
	if (msg->msg_namelen != sizeof(sin)) {
	    errno = EINVAL;
	    return -1;
	}

	a.size = offsetof(struct netd_op_args, sendto) +
		 offsetof(struct netd_op_sendto_args, buf) + iovbytes;
	a.op_type = netd_op_sendto;
	a.sendto.fd = fd->fd_sock.s;
	a.sendto.count = iovbytes;
	a.sendto.flags = flags;
	memcpy(&sin, msg->msg_name, msg->msg_namelen);
	libc_to_netd(&sin, &a.sendto.sin);

	for (uint32_t i = 0; i < msg->msg_iovlen; i++) {
	    memcpy(&a.sendto.buf[cc], msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
	    cc += msg->msg_iov[i].iov_len;
	}
    } else {
	a.size = offsetof(struct netd_op_args, send) +
		 offsetof(struct netd_op_send_args, buf) + iovbytes;
	a.op_type = netd_op_send;
	a.send.fd = fd->fd_sock.s;
	a.send.count = iovbytes;
	a.send.flags = flags;

	for (uint32_t i = 0; i < msg->msg_iovlen; i++) {
	    memcpy(&a.send.buf[cc], msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
	    cc += msg->msg_iov[i].iov_len;
	}
    }

    return netd_call(fd, &a);
}

static ssize_t
sock_sendto(struct Fd *fd, const void *buf, size_t count, int flags,
	    const struct sockaddr *to, socklen_t tolen)
{
    if (count > netd_buf_size) {
	if (to) {
	    errno = EMSGSIZE;
	    return -1;
	} else {
	    count = netd_buf_size;
	}
    }

    struct netd_op_args a;
    struct sockaddr_in sin;
    if (to) {
	if (tolen < sizeof(sin)) {
	    errno = EINVAL;
	    return -1;
	}

	memcpy(&sin, to, sizeof(sin));

	a.size = offsetof(struct netd_op_args, sendto) +
		 offsetof(struct netd_op_sendto_args, buf) + count;
	a.op_type = netd_op_sendto;
	a.sendto.fd = fd->fd_sock.s;
	a.sendto.count = count;
	a.sendto.flags = flags;
	libc_to_netd(&sin, &a.sendto.sin);
	memcpy(&a.sendto.buf[0], buf, count);
    } else {
	a.size = offsetof(struct netd_op_args, send) +
		 offsetof(struct netd_op_send_args, buf) + count;
	a.op_type = netd_op_send;
	a.send.fd = fd->fd_sock.s;
	a.send.count = count;
	a.send.flags = flags;
	memcpy(&a.send.buf[0], buf, count);
    }

    return netd_call(fd, &a);
}

static ssize_t
sock_recvfrom(struct Fd *fd, void *buf, size_t count, int flags, 
	      struct sockaddr *addr, socklen_t *addrlen)
{
    if (count > netd_buf_size)
	count = netd_buf_size;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, recvfrom) +
	     offsetof(struct netd_op_recvfrom_args, buf);

    a.op_type = netd_op_recvfrom;
    a.recvfrom.fd = fd->fd_sock.s;
    a.recvfrom.wantfrom = (addr ? 1 : 0);
    a.recvfrom.count = count;
    a.recvfrom.flags = flags;

    if (fd->fd_omode & O_NONBLOCK)
	a.recvfrom.flags |= MSG_DONTWAIT;

    int r = netd_call(fd, &a);
    if (r > 0) {
	memcpy(buf, &a.recvfrom.buf[0], r);
	if (addr) {
	    netd_to_libc(&a.recvfrom.sin, (struct sockaddr_in *) addr);
	    *addrlen = sizeof(struct sockaddr_in);
	}
    }
    return r;
}

static ssize_t
sock_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return sock_sendto(fd, buf, count, 0, 0, 0);
}

static ssize_t
sock_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return sock_recvfrom(fd, buf, count, 0, 0, 0);
}

static int
sock_close(struct Fd *fd)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, close) + sizeof(a.close);
    
    a.op_type = netd_op_close;
    a.close.fd = fd->fd_sock.s;
    return netd_call(fd, &a);
}

static int
sock_shutdown(struct Fd *fd, int how)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, shutdown) + sizeof(a.shutdown);

    a.op_type = netd_op_shutdown;
    a.shutdown.fd = fd->fd_sock.s;
    a.shutdown.how = how;
    return netd_call(fd, &a);
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
    int r = netd_call(fd, &a);
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
    int r = netd_call(fd, &a);
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

    a.op_type = netd_op_setsockopt;
    a.setsockopt.level = level;
    a.setsockopt.optname = optname;
    memcpy(&a.setsockopt.optval[0], optval, optlen);
    a.setsockopt.optlen = optlen;
    a.setsockopt.fd = fd->fd_sock.s;
    return netd_call(fd, &a);
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
    int r = netd_call(fd, &a);

    if (a.getsockopt.optlen > *optlen)
	return -E_INVAL;
    *optlen = a.getsockopt.optlen;
    memcpy(optval, &a.getsockopt.optval[0], a.getsockopt.optlen);
    return r;
}

static int
sock_stat(struct Fd *fd, struct stat64 *buf)
{
    buf->st_mode |= __S_IFSOCK;
    return 0;
}

static int
sock_probe(struct Fd *fd, dev_probe_t probe)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, probe) + sizeof(a.probe);

    a.op_type = netd_op_probe;
    a.probe.fd = fd->fd_sock.s;
    a.probe.how = probe;
    return netd_call(fd, &a);
}

static int
sock_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{
    int r;
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, statsync) + sizeof(a.statsync);

    a.op_type = netd_op_statsync;
    a.statsync.how = probe;
    a.statsync.fd = fd->fd_sock.s;
    
    r = netd_call(fd, &a);
    if (r < 0)
	return r;
    memcpy(wstat, &a.statsync.wstat, sizeof(*wstat));
    return r;
}

static int
sock_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, ioctl) + sizeof(a.ioctl);
    a.op_type = netd_op_ioctl;
    struct netd_op_ioctl_args *ia = (struct netd_op_ioctl_args *) &a.ioctl;

    switch (req) {
    case SIOCGIFCONF: {
	struct ifconf *ifc = va_arg(ap, struct ifconf *);
	if ((uint32_t)ifc->ifc_len < sizeof(struct ifreq)) {
	    errno = ENOBUFS;
	    return -1;
	}
	struct ifreq *r = (struct ifreq *)ifc->ifc_buf;

	ia->libc_ioctl = SIOCGIFCONF;
	int z = netd_call(fd, &a);
	if (z < 0)
	    return z;

	if (ia->gifconf.name[0] == 0) {
	    ifc->ifc_len = 0;	    
	    return 0;
	}
	
	int n = sizeof(r->ifr_name);
	strncpy(r->ifr_name, ia->gifconf.name, n);
	r->ifr_name[n - 1] = 0;

	netd_to_libc(&ia->gifconf.addr, (struct sockaddr_in *)&r->ifr_addr);
	ifc->ifc_len = sizeof(struct ifreq);
	return 0;
    }
    case SIOCGIFFLAGS: {
	struct ifreq *r = va_arg(ap, struct ifreq *);
	ia->libc_ioctl = SIOCGIFFLAGS;
	strncpy(ia->gifflags.name, r->ifr_name, sizeof(ia->gifbrdaddr.name));
	int z = netd_call(fd, &a);
	if (z < 0)
	    return z;

	r->ifr_flags = ia->gifflags.flags;
	return 0;
    }
    case SIOCGIFBRDADDR: {
	struct ifreq *r = va_arg(ap, struct ifreq *);
	ia->libc_ioctl = SIOCGIFBRDADDR;
	strncpy(ia->gifbrdaddr.name, r->ifr_name, sizeof(ia->gifbrdaddr.name));
	int z = netd_call(fd, &a);
	if (z < 0)
	    return z;
	
	netd_to_libc(&ia->gifbrdaddr.baddr, (struct sockaddr_in *) &r->ifr_broadaddr);
	return 0;
    }
    case SIOCGIFHWADDR: {
	struct ifreq *r = va_arg(ap, struct ifreq *);
	ia->libc_ioctl = SIOCGIFHWADDR;
	strncpy(ia->gifhwaddr.name, r->ifr_name, sizeof(ia->gifhwaddr.name));
	int z = netd_call(fd, &a);
	if (z < 0)
	    return z;
	
	struct sockaddr *sa = &r->ifr_hwaddr;
	sa->sa_family = ia->gifhwaddr.hwfamily;
	memcpy(sa->sa_data, ia->gifhwaddr.hwaddr, ia->gifhwaddr.hwlen);
	return 0;
    }
    default:
	fprintf(stderr, "sock_ioctl: unimplemented 0x%lx\n", req);
	errno = ENOSYS;
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
    .dev_recvfrom = sock_recvfrom,
    .dev_sendto = sock_sendto,
    .dev_sendmsg = sock_sendmsg,
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
