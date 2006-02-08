#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/gate.h>

#include <lwip/sockets.h>

static struct u_gate_entry netd_gate;

static void
netd_dispatch(struct netd_op_args *a)
{
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
}

static void
netd_gate_entry(void *x, struct cobj_ref *arg)
{
    struct netd_op_args *netd_op = 0;
    int r = segment_map(*arg, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_dispatch(netd_op);

    segment_unmap(netd_op);
}

int
netd_server_init(uint64_t ct)
{
    int r = gate_create(&netd_gate, ct, &netd_gate_entry, 0, "netd");
    return r;
}
