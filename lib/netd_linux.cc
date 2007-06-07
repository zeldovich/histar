extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/netdlinux.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/netdsrv.hh>

enum { netd_do_taint = 0 };

static void __attribute__((noreturn))
netd_linux_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    netd_socket_handler h = (netd_socket_handler) a;
    socket_conn *sr = (socket_conn *)gcd->param_buf;
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
	gd.name_ = "netd-socket";
	gd.func_ = &netd_linux_gate_entry;
	gd.flags_ = GATESRV_KEEP_TLS_STACK;
	cobj_ref gate = gate_create(&gd);
	
	cprintf("gate: %ld.%ld\n", gate.container, gate.object);

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
