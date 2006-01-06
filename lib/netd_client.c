#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/gate.h>
#include <inc/fd.h>

static int netd_client_inited;
static struct cobj_ref netd_gate;

static int
netd_client_init()
{
    uint64_t rc = start_env->root_container;
    int64_t nslots = sys_container_nslots(rc);
    if (nslots < 0)
	return nslots;

    for (uint64_t i = 0; i < nslots; i++) {
	int64_t id = sys_container_get_slot_id(rc, i);
	if (id < 0)
	    continue;

	kobject_type_t type = sys_obj_get_type(COBJ(rc, id));
	if (type < 0)
	    return type;

	if (type == kobj_gate) {
	    netd_gate = COBJ(rc, id);
	    netd_client_inited = 1;
	    return 0;
	}
    }

    return -1;
}

static int
netd_call(struct netd_op_args *a) {
    for (int i = 0; i < 10 && netd_client_inited == 0; i++) {
	int r = netd_client_init();
	if (r < 0)
	    sys_thread_sleep(100);
    }

    if (netd_client_inited == 0) {
	cprintf("netd_call: cannot initialize netd client\n");
	return -1;
    }

    struct cobj_ref seg;
    void *va = 0;
    int r = segment_alloc(start_env->container, PGSIZE, &seg, &va);
    if (r < 0)
	return r;

    memcpy(va, a, sizeof(*a));
    gate_call(start_env->container, netd_gate, &seg);

    memcpy(a, va, sizeof(*a));
    int rval = a->rval;

    segment_unmap(va);
    sys_obj_unref(seg);
    return rval;
}

int
socket(int domain, int type, int protocol)
{
    struct Fd *fd;
    int r = fd_alloc(start_env->container, &fd);
    if (r < 0)
	return r;

    struct netd_op_args a;
    a.op_type = netd_op_socket;
    a.args.socket.domain = domain;
    a.args.socket.type = type;
    a.args.socket.protocol = protocol;
    int sock = netd_call(&a);

    if (sock < 0) {
	fd_close(fd);
	return sock;
    }

    fd->fd_dev_id = devsock.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_data.sock.s = sock;
    return fd2num(fd);
}

int
sock_bind(struct Fd *fd, struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    if (addrlen != sizeof(a.args.bind.sin))
	return -E_INVAL;

    a.op_type = netd_op_bind;
    a.args.bind.fd = fd->fd_data.sock.s;
    memcpy(&a.args.bind.sin, addr, addrlen);
    return netd_call(&a);
}

int
sock_listen(struct Fd *fd, int backlog)
{
    struct netd_op_args a;
    a.op_type = netd_op_listen;
    a.args.listen.fd = fd->fd_data.sock.s;
    a.args.listen.backlog = backlog;
    return netd_call(&a);
}

int
sock_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct netd_op_args a;
    if (*addrlen != sizeof(a.args.accept.sin))
	return -E_INVAL;

    struct Fd *nfd;
    int r = fd_alloc(start_env->container, &nfd);
    if (r < 0)
	return r;

    a.op_type = netd_op_accept;
    a.args.accept.fd = fd->fd_data.sock.s;
    memcpy(&a.args.accept.sin, addr, *addrlen);
    int sock = netd_call(&a);

    if (sock < 0) {
	fd_close(nfd);
	return sock;
    }

    memcpy(addr, &a.args.accept.sin, *addrlen);
    nfd->fd_dev_id = devsock.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_data.sock.s = sock;

    return fd2num(nfd);
}

int
sock_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    if (count > 1024)
	count = 1024;

    struct netd_op_args a;
    a.op_type = netd_op_write;
    a.args.write.fd = fd->fd_data.sock.s;
    a.args.write.count = count;
    memcpy(&a.args.write.buf[0], buf, count);
    return netd_call(&a);
}

int
sock_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    if (count > 1024)
	count = 1024;

    struct netd_op_args a;
    a.op_type = netd_op_read;
    a.args.read.fd = fd->fd_data.sock.s;
    a.args.read.count = count;
    int r = netd_call(&a);
    if (r > 0)
	memcpy(buf, &a.args.read.buf[0], r);
    return r;
}

int
sock_close(struct Fd *fd)
{
    struct netd_op_args a;
    a.op_type = netd_op_close;
    a.args.close.fd = fd->fd_data.sock.s;
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
    .dev_listen = sock_listen,
    .dev_accept = sock_accept
};
