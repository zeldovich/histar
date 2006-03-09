#include <inc/assert.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/netd.h>

#include <lwip/sockets.h>
#include <arch/sys_arch.h>

static void
netd_to_lwip(struct netd_sockaddr_in *nsin, struct sockaddr_in *sin)
{
    sin->sin_family = AF_INET;
    sin->sin_port = nsin->sin_port;
    sin->sin_addr.s_addr = nsin->sin_addr;
}

static void
lwip_to_netd(struct sockaddr_in *sin, struct netd_sockaddr_in *nsin)
{
    nsin->sin_port = sin->sin_port;
    nsin->sin_addr = sin->sin_addr.s_addr;
}

void
netd_dispatch(struct netd_op_args *a)
{
    lwip_core_lock();

    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);

    switch (a->op_type) {
    case netd_op_socket:
	a->rval = lwip_socket(a->socket.domain,
			      a->socket.type,
			      a->socket.protocol);
	break;

    case netd_op_bind:
	netd_to_lwip(&a->bind.sin, &sin);

	a->rval = lwip_bind(a->bind.fd,
			    (struct sockaddr *) &sin, sinlen);
	break;

    case netd_op_connect:
	netd_to_lwip(&a->connect.sin, &sin);

	a->rval = lwip_connect(a->connect.fd,
			       (struct sockaddr *) &sin, sinlen);
	break;

    case netd_op_listen:
	a->rval = lwip_listen(a->listen.fd,
			      a->listen.backlog);
	break;

    case netd_op_accept:
	a->rval = lwip_accept(a->accept.fd,
			      (struct sockaddr *) &sin, &sinlen);
	lwip_to_netd(&sin, &a->accept.sin);
	break;

    case netd_op_read:
	a->rval = lwip_read(a->read.fd,
			    &a->read.buf[0],
			    a->read.count);
	break;

    case netd_op_write:
	a->rval = lwip_write(a->write.fd,
			     &a->write.buf[0],
			     a->write.count);
	break;

    case netd_op_close:
	a->rval = lwip_close(a->close.fd);
	break;

    default:
	cprintf("netd_dispatch: unknown netd op %d\n", a->op_type);
	a->rval = -E_INVAL;
    }

    lwip_core_unlock();
}
