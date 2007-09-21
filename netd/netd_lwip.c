#include <inc/assert.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <netd/netdlwip.h>

#include <lwip/sockets.h>
#include <arch/sys_arch.h>
#include <api/ext.h>

#include <stddef.h>

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
netd_lwip_dispatch(struct netd_op_args *a)
{
    lwip_core_lock();

    int err_fd = -1;

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

	err_fd = a->bind.fd;
	a->rval = lwip_bind(a->bind.fd,
			    (struct sockaddr *) &sin, sinlen);
	break;

    case netd_op_connect:
	netd_to_lwip(&a->connect.sin, &sin);

	err_fd = a->connect.fd;
	a->rval = lwip_connect(a->connect.fd,
			       (struct sockaddr *) &sin, sinlen);
	break;

    case netd_op_listen:
	err_fd = a->listen.fd;
	a->rval = lwip_listen(a->listen.fd,
			      a->listen.backlog);
	break;

    case netd_op_accept:
	err_fd = a->accept.fd;
	a->rval = lwip_accept(a->accept.fd,
			      (struct sockaddr *) &sin, &sinlen);
	lwip_to_netd(&sin, &a->accept.sin);
	break;

    case netd_op_recvfrom:
	err_fd = a->recvfrom.fd;
	a->rval = lwip_recvfrom(a->recvfrom.fd, &a->recvfrom.buf[0],
				a->recvfrom.count, a->recvfrom.flags,
				a->recvfrom.wantfrom ? ((struct sockaddr *) &sin) : 0,
				&sinlen);
	if (a->recvfrom.wantfrom)
	    lwip_to_netd(&sin, &a->recvfrom.sin);
	if (a->rval > 0)
	    a->size = offsetof(struct netd_op_args, recvfrom) +
		      offsetof(struct netd_op_recvfrom_args, buf) + a->rval;
	break;

    case netd_op_send:
	err_fd = a->send.fd;
	a->rval = lwip_send(a->send.fd,
			    &a->send.buf[0],
			    a->send.count, a->send.flags);
	a->size = offsetof(struct netd_op_args, send) +
		  offsetof(struct netd_op_send_args, buf);
	break;

    case netd_op_sendto:
	netd_to_lwip(&a->sendto.sin, &sin);
	err_fd = a->sendto.fd;
	a->rval = lwip_sendto(a->sendto.fd,
			      &a->sendto.buf[0],
			      a->sendto.count, a->sendto.flags,
			      (struct sockaddr *)&sin, sinlen);
	a->size = offsetof(struct netd_op_args, sendto) +
		  offsetof(struct netd_op_sendto_args, buf);
	break;
	
    case netd_op_close:
	err_fd = a->close.fd;
	a->rval = lwip_close(a->close.fd);
	break;

    case netd_op_getsockname:
	err_fd = a->getsockname.fd;
        a->rval = lwip_getsockname(a->getsockname.fd, 
                                   (struct sockaddr *) &sin, &sinlen);
	lwip_to_netd(&sin, &a->getsockname.sin);
        break ;
    
    case netd_op_getpeername:
	err_fd = a->getpeername.fd;
        a->rval = lwip_getpeername(a->getpeername.fd, 
                                   (struct sockaddr *) &sin, &sinlen);
	lwip_to_netd(&sin, &a->getpeername.sin);
        break ;

    case netd_op_setsockopt:
	err_fd = a->setsockopt.fd;

	if (a->setsockopt.level == SOL_SOCKET) {
	    /* LWIP does not supoport and some packages error out 
	     * if REUSEADDR fails 
	     */
	    if (a->setsockopt.optname == SO_REUSEADDR) {
		a->rval = 0;
		break;
	    }
	}

        a->rval = lwip_setsockopt(a->setsockopt.fd,
                                  a->setsockopt.level,
                                  a->setsockopt.optname,
                                  &a->setsockopt.optval[0],
                                  a->setsockopt.optlen);
        break;

    case netd_op_getsockopt:
	err_fd = a->getsockopt.fd;
	a->getsockopt.optlen = sizeof(a->getsockopt.optval);
        a->rval = lwip_getsockopt(a->getsockopt.fd, 
                                  a->getsockopt.level,
                                  a->getsockopt.optname,
                                  &a->getsockopt.optval[0],
                                  &a->getsockopt.optlen);
        break;

    case netd_op_notify: {
	char write = a->notify.how == dev_probe_write;
	a->rval = lwipext_sync_waiting(a->notify.fd, write);
	break;
    }
    case netd_op_probe: {
	err_fd = a->probe.fd;
	
	fd_set set;
	FD_ZERO(&set);
	FD_SET(a->probe.fd, &set);
	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = 0;

	if (a->probe.how == dev_probe_write)
	    a->rval = lwip_select(a->probe.fd + 1, 0, &set, 0, &to);
	else 
	    a->rval = lwip_select(a->probe.fd + 1, &set, 0, 0, &to);

	break;
    }
    case netd_op_shutdown:
	err_fd = a->shutdown.fd;
	a->rval = lwip_shutdown(a->shutdown.fd, a->shutdown.how);
	break;

    default:
	cprintf("netd_dispatch: unknown netd op %d\n", a->op_type);
	a->rval = -E_INVAL;
    }

    if (a->rval < 0 && err_fd >= 0) {
	socklen_t len = sizeof(a->rerrno);
	lwip_getsockopt(err_fd, SOL_SOCKET, SO_ERROR, &a->rerrno, &len);
    } else {
	a->rerrno = 0;
    }

    lwip_core_unlock();
}
