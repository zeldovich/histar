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

void
netd_dispatch(struct netd_op_args *a)
{
    lwip_core_lock();

    switch (a->op_type) {
    case netd_op_socket:
	a->rval = lwip_socket(a->socket.domain,
			      a->socket.type,
			      a->socket.protocol);
	break;

    case netd_op_bind:
	a->rval = lwip_bind(a->bind.fd,
			    (struct sockaddr*) &a->bind.sin,
			    sizeof(a->bind.sin));
	break;

    case netd_op_connect:
	a->rval = lwip_connect(a->connect.fd,
			       (struct sockaddr *) &a->connect.sin,
			       sizeof(a->connect.sin));
	break;

    case netd_op_listen:
	a->rval = lwip_listen(a->listen.fd,
			      a->listen.backlog);
	break;

    case netd_op_accept:
	{
	    socklen_t sinlen = sizeof(a->accept.sin);
	    a->rval = lwip_accept(a->accept.fd,
				  (struct sockaddr*) &a->accept.sin,
				  &sinlen);
	}
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
