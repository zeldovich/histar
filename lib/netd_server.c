#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/gate.h>

#include <lwip/sockets.h>

static struct u_gate_entry netd_gate;
static int64_t netd_ct;

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
    struct ulabel *l = label_get_obj(*arg);
    if (l == 0)
	panic("netd_gate_entry: cannot get label for args segment");

    int64_t arg_copy_id = sys_segment_copy(*arg, netd_ct,
					   segment_get_default_label(),
					   "netd_gate_entry() args");
    if (arg_copy_id < 0)
	panic("netd_gate_entry: cannot copy args: %s", e2s(arg_copy_id));
    sys_obj_unref(*arg);

    struct cobj_ref arg_copy = COBJ(netd_ct, arg_copy_id);
    struct netd_op_args *netd_op = 0;
    int r = segment_map(arg_copy, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_dispatch(netd_op);
    segment_unmap(netd_op);

    uint64_t copy_back_ct = kobject_id_thread_ct;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct,
					    l, "netd_gate_entry() reply");
    if (copy_back_id < 0)
	panic("netd_gate_entry: cannot copy back: %s", e2s(copy_back_id));
    sys_obj_unref(arg_copy);

    *arg = COBJ(copy_back_ct, copy_back_id);
}

int
netd_server_init(uint64_t ct)
{
    netd_ct = container_find(ct, kobj_container, "netd gate");
    if (netd_ct < 0)
	return netd_ct;

    int r = gate_create(&netd_gate, netd_ct, &netd_gate_entry, 0, "netd");
    return r;
}
