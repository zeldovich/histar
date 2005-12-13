#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/gate.h>

#include <lwip/sockets.h>

static struct u_gate_entry netd_gate;
static uint64_t ctemp;

static void
netd_dispatch(struct netd_op_args *a)
{
    switch (a->op_type) {
    case netd_op_socket:
	a->rval = socket(a->args.socket.domain,
			 a->args.socket.type,
			 a->args.socket.protocol);
	break;

    case netd_op_bind:
	a->rval = bind(a->args.bind.fd,
		       (struct sockaddr*) &a->args.bind.sin,
		       sizeof(a->args.bind.sin));
	break;

    case netd_op_listen:
	a->rval = listen(a->args.listen.fd,
			 a->args.listen.backlog);
	break;

    case netd_op_accept:
	{
	    socklen_t sinlen = sizeof(a->args.accept.sin);
	    a->rval = accept(a->args.accept.fd,
			     (struct sockaddr*) &a->args.accept.sin,
			     &sinlen);
	}
	break;

    case netd_op_write:
	a->rval = write(a->args.write.fd,
			&a->args.write.buf[0],
			a->args.write.count);
	break;

    case netd_op_close:
	a->rval = close(a->args.close.fd);
	break;

    default:
	cprintf("netd_dispatch: unknown netd op %d\n", a->op_type);
	a->rval = -E_INVAL;
    }
}

static void
netd_gate_entry(void *x, struct cobj_ref *arg)
{
    struct netd_op_args *netd_op;
    int r = segment_map(ctemp, *arg, 1, (void**)&netd_op, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %e\n", e2s(r));

    netd_dispatch(netd_op);

    segment_unmap(ctemp, netd_op);
}

int
netd_server_init(uint64_t ct)
{
    ctemp = ct;
    int r = gate_create(&netd_gate, ct, &netd_gate_entry, 0);
    return r;
}
