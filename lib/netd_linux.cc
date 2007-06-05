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
netd_jcomm_gate_entry(uint64_t a, gate_call_data *gcd, gatesrv_return *rg)
{
    netd_jcomm_handler h = (netd_jcomm_handler)a;

    jcomm_ref jr;
    int *ret = (int *)gcd->param_buf;
    memcpy(&jr, gcd->param_buf, sizeof(jr));
    
    *ret = h(jr);
    
    rg->ret(0, 0, 0);
}

int
netd_linux_server_init(netd_jcomm_handler h)
{
    try {
	label cntm;
	label clear;
	uint64_t inet_taint = 0;
	
	thread_cur_label(&cntm);
	thread_cur_clearance(&clear);
	if (netd_do_taint)
	    cntm.set(inet_taint, 2);

	gate_create(start_env->shared_container, "netd-jcomm", &cntm, &clear, 
		    0, netd_jcomm_gate_entry, (uintptr_t)h);

	/*
	netd_server_init(start_env->shared_container,
			 inet_taint, &cntm, &clear, h);
	netd_server_enable();
	*/

	// Disable signals -- the signal gate has { inet_taint:* }
	int64_t sig_gt = container_find(start_env->shared_container, kobj_gate, "signal");
	error_check(sig_gt);
	error_check(sys_obj_unref(COBJ(start_env->shared_container, sig_gt)));

	thread_set_label(&cntm);
    } catch (std::exception &e) {
	panic("%s", e.what());
    }
    
    return 0;
}
