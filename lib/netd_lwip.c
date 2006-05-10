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
netd_dispatch(struct netd_op_args *a)
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

    case netd_op_recv:
	{
	    a->rval = 0;

	    err_fd = a->recv.fd;
	    while (!a->recv.flags && a->rval < (ssize_t) a->recv.count) {
		ssize_t cc = lwip_recv(a->recv.fd, &a->recv.buf[a->rval],
				       a->recv.count - a->rval,
				       MSG_DONTWAIT | a->recv.flags);
		if (cc <= 0)
		    break;

		a->rval += cc;
	    }

	    if (a->rval == 0) {
		a->rval = lwip_recv(a->recv.fd, &a->recv.buf[0],
				    a->recv.count, a->recv.flags);
	    }

	    if (a->rval > 0)
		a->size = offsetof(struct netd_op_args, recv) +
			  offsetof(struct netd_op_recv_args, buf) + a->rval;
	}
	break;

    case netd_op_send:
	err_fd = a->send.fd;
	a->rval = lwip_send(a->send.fd,
			    &a->send.buf[0],
			    a->send.count, a->send.flags);
	a->size = offsetof(struct netd_op_args, recv) +
		  offsetof(struct netd_op_send_args, buf);
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

    case netd_op_select:
	{
	    fd_set set;
	    FD_ZERO(&set);
	    FD_SET(a->select.fd, &set);
	    struct timeval tv = {0, 0};

	    err_fd = a->select.fd;
	    if (a->select.write)
		a->rval = lwip_select(a->select.fd + 1, 0, &set, 0, &tv);
	    else
		a->rval = lwip_select(a->select.fd + 1, &set, 0, 0, &tv);

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
	socklen_t len = sizeof(a->errno);
	lwip_getsockopt(err_fd, SOL_SOCKET, SO_ERROR, &a->errno, &len);
    } else {
	a->errno = 0;
    }

    lwip_core_unlock();
}
