extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <netd/netdlinux.h>

#include <errno.h>
#include <inttypes.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/gobblegateclnt.hh>
#include <netd/netdsrv.hh>

enum { netd_do_taint = 0 };

static int
gate_lookup(const char *bn, const char *gn, struct cobj_ref *ret)
{
    struct fs_inode netd_ct_ino;
    int r = fs_namei(bn, &netd_ct_ino);
    if (r == 0) {
	uint64_t netd_ct = netd_ct_ino.obj.object;
	
	int64_t gate_id = container_find(netd_ct, kobj_gate, gn);
	if (gate_id > 0) {
	    *ret = COBJ(netd_ct, gate_id);
	    return 0;
	}
    }
    return -E_NOT_FOUND;
}

static void
netd_linux_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    int r = 0;
    netd_socket_handler h = (netd_socket_handler) a;
    socket_conn *sr = (socket_conn *)gcd->param_buf;
    /* let our caller know we are clear */
    int64_t z = jcomm_write(sr->ctrl_comm, &r, sizeof(r));
    if (z < 0) 
	cprintf("netd_lnux_gate_entry: jcomm_write error: %"PRIu64"\n", z);
    h(sr);
}

int
netd_linux_server_init(netd_socket_handler h)
{
    try {
	label l(1);
	label c(3);
	label v(3);
	
	thread_cur_label(&l);
	thread_cur_clearance(&c);

	uint64_t inet_taint = 0;
	if (netd_do_taint)
	    l.set(inet_taint, 2);
	
	
	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.label_ = &l;
	gd.clearance_ = &c;
	gd.verify_ = &v;

	gd.arg_ = (uintptr_t) h;
	gd.name_ = "netd";
	gd.func_ = &netd_linux_gate_entry;
	gd.flags_ = GATESRV_KEEP_TLS_STACK;
	cobj_ref gate = gate_create(&gd);
	
	int64_t sig_gt = container_find(start_env->shared_container, kobj_gate, "signal");
	error_check(sig_gt);
	error_check(sys_obj_unref(COBJ(start_env->shared_container, sig_gt)));
	
	thread_set_label(&l);
    } catch (std::exception &e) {
	cprintf("netd_linux_server_init: %s\n", e.what());
	return -1;
    }
    return 0;
}

static int
setup_socket_conn(cobj_ref gate, struct socket_conn *client_conn, int sock_id)
{
    int r;
    /* allocate some args */
    uint64_t taint = handle_alloc();
    uint64_t grant = handle_alloc();

    label l(1);
    l.set(taint, 3);
    l.set(grant, 0);
    
    int64_t ct = sys_container_alloc(start_env->shared_container, l.to_ulabel(),
				     "socket-store", 0, CT_QUOTA_INF);
    if (ct < 0)
	return ct;
    jcomm_ref ctrl_comm0, ctrl_comm1;
    r = jcomm_alloc(ct, l.to_ulabel(), JCOMM_PACKET, 
		    &ctrl_comm0, &ctrl_comm1);
    if (r < 0)
	return r;

    jcomm_ref data_comm0, data_comm1;
    /* XXX need to be packet if DGRAM */
    r = jcomm_alloc(ct, l.to_ulabel(), 0, &data_comm0, &data_comm1);
    if (r < 0) {
	jcomm_unref(ctrl_comm0);
	return r;
    }
    
    gate_call_data gcd;
    socket_conn *sc = (socket_conn *)gcd.param_buf;
    
    sc->container = ct;
    sc->taint = taint;
    sc->grant = grant;
    sc->ctrl_comm = ctrl_comm1;
    sc->data_comm = data_comm1;
    sc->init_magic = NETD_LINUX_MAGIC;
    sc->sock_id = sock_id;
    
    label ds(3);
    ds.set(taint, LB_LEVEL_STAR);
    ds.set(grant, LB_LEVEL_STAR);

    try {
	/* clean up thread artifacts in destructor */
	gobblegate_call gc(gate, 0, &ds, 0, 1);
	gc.call(&gcd, 0, 0);

	client_conn->container = ct;
	client_conn->taint = taint;
	client_conn->grant = grant;
	client_conn->ctrl_comm = ctrl_comm0;
	client_conn->data_comm = data_comm0;
	
	/* need to wait for thread signal, so can safely cleanup */
	int64_t z = jcomm_read(ctrl_comm0, &r, sizeof(r));
	if (z < 0) { 
	    cprintf("setup_socket_conn: jcomm_read error: %"PRIu64"\n", z);
	    return z;
	}
	else if (r < 0) {
	    cprintf("setup_socket_conn: gobble thread error: %d\n", r);
	    return r;
	}
    } catch (std::exception &e) {
	cprintf("setup_socket_conn: gobblegate call error: %s\n", e.what());
	return -1;
    }
    client_conn->init_magic = NETD_LINUX_MAGIC;
    return 0;
}

int
netd_linux_client_init(struct cobj_ref *gate)
{
    return gate_lookup("/vmlinux", "netd", gate);
}

int 
netd_linux_call(struct Fd *fd, struct netd_op_args *a)
{
    int r;
    int64_t z;
    struct socket_conn *client_conn = (struct socket_conn *) fd->fd_sock.extra;

    if (client_conn->init_magic != NETD_LINUX_MAGIC) {
	/* if we aren't creating a new socket we probably are socket 
	 * created by accept that hasn't been used yet
	 */
	if (a->op_type == netd_op_socket)
	    r = setup_socket_conn(fd->fd_sock.netd_gate, client_conn, -1);
	else
	    r = setup_socket_conn(fd->fd_sock.netd_gate, client_conn, fd->fd_sock.s);

	if (r < 0)
	    return r;
	fd_set_extra_handles(fd, client_conn->grant, client_conn->taint);
    }
    
    switch(a->op_type) {
    case netd_op_close:
	/* Linux doesn't send a response on close.  We send the close 
	 * operation over the jcomm to pop Linux out of multisync.
	 */
	z = jcomm_write(client_conn->ctrl_comm, a, a->size);
	assert(z == a->size);
	r = jcomm_shut(client_conn->ctrl_comm, JCOMM_SHUT_RD | JCOMM_SHUT_WR);
	if (r < 0)
	    cprintf("netd_linux_call: jcomm_shut error: %s\n", e2s(r));
	jcomm_shut(client_conn->data_comm, JCOMM_SHUT_RD | JCOMM_SHUT_WR);
	r = sys_obj_unref(COBJ(start_env->shared_container, client_conn->container));
	if (r < 0)
	    cprintf("netd_linux_call: sys_obj_unref error: %s\n", e2s(r));
	return 0;
    case netd_op_probe: 
	/* XXX how to handle selecting on a listening socket */
	return jcomm_probe(client_conn->data_comm, a->probe.how);
    case netd_op_statsync:
	return jcomm_multisync(client_conn->data_comm, 
			       a->statsync.how, 
			       &a->statsync.wstat);
    case netd_op_recvfrom:
	if (!a->recvfrom.wantfrom) {
	    r = jcomm_read(client_conn->data_comm, a->recvfrom.buf, a->recvfrom.count);
	    if (r < 0) {
		cprintf("netd_linux_call: jcomm_read error: %s\n", e2s(r));
		errno = ENOSYS;
		return -1;
	    }
	    return r;
	}
	errno = ENOSYS;
	return -1;
    case netd_op_accept:
	r = jcomm_read(client_conn->data_comm, &a->accept, sizeof(a->accept));
	if (r < 0) {
	    cprintf("netd_linux_call: jcomm_read error: %s\n", e2s(r));
	    errno = ENOSYS;
	    return -1;
	}
	return a->accept.fd;
    case netd_op_socket:
    case netd_op_connect:
    case netd_op_send:
    case netd_op_sendto:
    case netd_op_setsockopt:
    case netd_op_bind:
    case netd_op_listen:
    case netd_op_ioctl:
	break;
    default:
	cprintf("netd_linux_call: unimplemented %d\n", a->op_type);
	errno = ENOSYS;
	return -1;
    }
    
    /* write operation request */
    z = jcomm_write(client_conn->ctrl_comm, a, a->size);
    assert(z == a->size);

    /* read return value */
    z = jcomm_read(client_conn->ctrl_comm, a, sizeof(*a));
    assert(z == a->size);
    if (a->rval < 0) {
	errno = -1 * a->rval;
	return -1;
    }
    return a->rval;
}
