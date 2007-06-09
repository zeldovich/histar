extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/netdioctl.h>
#include <inc/netdlinux.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/fd.h>

#include <errno.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/netdsrv.hh>
#include <inc/gobblegateclnt.hh>

enum { netd_do_taint = 0 };

static void __attribute__((noreturn))
netd_linux_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    int r = 0;
    netd_socket_handler h = (netd_socket_handler) a;
    socket_conn *sr = (socket_conn *)gcd->param_buf;
    /* let our caller know we are clear */
    int64_t z = jcomm_write(sr->socket_comm, &r, sizeof(r));
    if (z < 0) 
	cprintf("netd_lnux_gate_entry: jcomm_write error: %ld\n", z);
    h(sr);

    rg->ret(0, 0, 0);
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
setup_socket_conn(cobj_ref gate, struct socket_conn *client_conn)
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
    jcomm_ref socket_comm0, socket_comm1;
    r = jcomm_alloc(ct, l.to_ulabel(), JCOMM_PACKET, 
		    &socket_comm0, &socket_comm1);
    if (r < 0)
	return r;

    gate_call_data gcd;
    socket_conn *sc = (socket_conn *)gcd.param_buf;
    
    sc->container = ct;
    sc->taint = taint;
    sc->grant = grant;
    sc->socket_comm = socket_comm1;

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
	client_conn->socket_comm = socket_comm0;
	
	/* need to wait for thread signal, so can safely cleanup */
	int64_t z = jcomm_read(socket_comm0, &r, sizeof(r));
	if (z < 0) { 
	    cprintf("setup_socket_conn: jcomm_read error: %ld\n", z);
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
    return 0;
}

int 
netd_linux_call(struct Fd *fd, struct netd_op_args *a)
{
    int r;
    int64_t z;
    struct socket_conn *client_conn = (struct socket_conn *) fd->fd_sock.extra;
    
    switch(a->op_type) {
    case netd_op_socket:
	r = setup_socket_conn(fd->fd_sock.netd_gate, client_conn);
	if (r < 0)
	    return r;
	fd_set_extra_handles(fd, client_conn->grant, client_conn->taint);
	break;
    case netd_op_close:
    default:
	panic("not implemented");
	break;
    }
    
    /* write operation request */
    z = jcomm_write(client_conn->socket_comm, a, a->size);
    assert(z == a->size);

    /* read return value */
    struct netd_linux_ret ret;
    z = jcomm_read(client_conn->socket_comm, &ret, sizeof(ret));
    assert(z == sizeof(ret));

    if (ret.rerrno < 0) {
	errno = ret.rerrno; 
	return -1;
    }
    return 0;
}

int 
netd_linux_ioctl(struct Fd *fd, struct netd_ioctl_args *a)
{
    return -1;
}
