#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fd.h>

const struct host_entry host_table[] = {
    { "market", "market.scs.stanford.edu" },
    { "market.scs.stanford.edu", "171.66.3.9" },

    { "suif", "suif.stanford.edu" },
    { "suif.stanford.edu", "171.64.73.155" },

    { 0, 0 }
};

int
socket(int domain, int type, int protocol)
{
    struct Fd *fd;
    int r = fd_alloc(start_env->proc_container, &fd, "socket fd");
    if (r < 0)
	return r;

    struct netd_op_args a;
    a.op_type = netd_op_socket;
    a.socket.domain = domain;
    a.socket.type = type;
    a.socket.protocol = protocol;
    int sock = netd_call(&a);

    if (sock < 0) {
	fd_close(fd);
	return sock;
    }

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_sock.s = sock;
    return fd2num(fd);
}

static int
sock_bind(struct Fd *fd, struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    if (addrlen != sizeof(a.bind.sin))
	return -E_INVAL;

    a.op_type = netd_op_bind;
    a.bind.fd = fd->fd_sock.s;
    memcpy(&a.bind.sin, addr, addrlen);
    return netd_call(&a);
}

static int
sock_connect(struct Fd *fd, struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    if (addrlen != sizeof(a.bind.sin))
	return -E_INVAL;

    a.op_type = netd_op_connect;
    a.connect.fd = fd->fd_sock.s;
    memcpy(&a.connect.sin, addr, addrlen);
    return netd_call(&a);
}

static int
sock_listen(struct Fd *fd, int backlog)
{
    struct netd_op_args a;
    a.op_type = netd_op_listen;
    a.listen.fd = fd->fd_sock.s;
    a.listen.backlog = backlog;
    return netd_call(&a);
}

static int
sock_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct netd_op_args a;
    if (*addrlen != sizeof(a.accept.sin))
	return -E_INVAL;

    struct Fd *nfd;
    int r = fd_alloc(start_env->proc_container, &nfd, "socket fd -- accept");
    if (r < 0)
	return r;

    a.op_type = netd_op_accept;
    a.accept.fd = fd->fd_sock.s;
    memcpy(&a.accept.sin, addr, *addrlen);
    int sock = netd_call(&a);

    if (sock < 0) {
	fd_close(nfd);
	return sock;
    }

    memcpy(addr, &a.accept.sin, *addrlen);
    nfd->fd_dev_id = devsock.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_sock.s = sock;

    return fd2num(nfd);
}

static int
sock_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    if (count > 1024)
	count = 1024;

    struct netd_op_args a;
    a.op_type = netd_op_write;
    a.write.fd = fd->fd_sock.s;
    a.write.count = count;
    memcpy(&a.write.buf[0], buf, count);
    return netd_call(&a);
}

static int
sock_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    if (count > 1024)
	count = 1024;

    struct netd_op_args a;
    a.op_type = netd_op_read;
    a.read.fd = fd->fd_sock.s;
    a.read.count = count;
    int r = netd_call(&a);
    if (r > 0)
	memcpy(buf, &a.read.buf[0], r);
    return r;
}

static int
sock_close(struct Fd *fd)
{
    struct netd_op_args a;
    a.op_type = netd_op_close;
    a.close.fd = fd->fd_sock.s;
    return netd_call(&a);
}

struct Dev devsock = 
{
    .dev_id = 's',
    .dev_name = "sock",
    .dev_read = sock_read,
    .dev_write = sock_write,
    .dev_close = sock_close,
    .dev_bind = sock_bind,
    .dev_connect = sock_connect,
    .dev_listen = sock_listen,
    .dev_accept = sock_accept
};
