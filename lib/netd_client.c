#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/gate.h>

static struct cobj_ref netd_gate;
static uint64_t ctemp;

int
netd_client_init(uint64_t ct)
{
    ctemp = ct;

    uint64_t rc = 1;
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
	    cprintf("netd_client_init: found gate <%ld.%ld>\n",
		    rc, id);
	    netd_gate = COBJ(rc, id);
	    return 0;
	}
    }

    cprintf("netd_client_init: cannot find gate\n");
    return -1;
}

static int
netd_call(struct netd_op_args *a) {
    struct cobj_ref seg;
    void *va;
    int r = segment_alloc(ctemp, PGSIZE, &seg, &va);
    if (r < 0)
	return r;

    memcpy(va, a, sizeof(*a));
    gate_call(ctemp, netd_gate, &seg);

    memcpy(a, va, sizeof(*a));
    int rval = a->rval;

    segment_unmap(va);
    sys_obj_unref(seg);
    return rval;
}

int netd_socket(int domain, int type, int protocol)
{
    struct netd_op_args a;
    a.op_type = netd_op_socket;
    a.args.socket.domain = domain;
    a.args.socket.type = type;
    a.args.socket.protocol = protocol;
    return netd_call(&a);
}

int
netd_bind(int fd, struct sockaddr *addr, socklen_t addrlen)
{
    struct netd_op_args a;
    if (addrlen != sizeof(a.args.bind.sin))
	return -E_INVAL;

    a.op_type = netd_op_bind;
    a.args.bind.fd = fd;
    memcpy(&a.args.bind.sin, addr, addrlen);
    return netd_call(&a);
}

int
netd_listen(int fd, int backlog)
{
    struct netd_op_args a;
    a.op_type = netd_op_listen;
    a.args.listen.fd = fd;
    a.args.listen.backlog = backlog;
    return netd_call(&a);
}

int
netd_accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct netd_op_args a;
    if (*addrlen != sizeof(a.args.accept.sin))
	return -E_INVAL;

    a.op_type = netd_op_accept;
    a.args.accept.fd = fd;
    memcpy(&a.args.accept.sin, addr, *addrlen);
    int r = netd_call(&a);
    memcpy(addr, &a.args.accept.sin, *addrlen);
    return r;
}

int
netd_write(int fd, const void *buf, size_t count)
{
    if (count > 1024)
	return -E_NO_SPACE;

    struct netd_op_args a;
    a.op_type = netd_op_write;
    a.args.write.fd = fd;
    a.args.write.count = count;
    memcpy(&a.args.write.buf[0], buf, count);
    return netd_call(&a);
}

int
netd_close(int fd)
{
    struct netd_op_args a;
    a.op_type = netd_op_close;
    a.args.close.fd = fd;
    return netd_call(&a);
}
