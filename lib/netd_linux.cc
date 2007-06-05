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

int
netd_linux_server_init(netd_handler h)
{
    try {
	label cntm;
	label clear;
	uint64_t inet_taint = 0;
	
	thread_cur_label(&cntm);
	thread_cur_clearance(&clear);
	if (netd_do_taint)
	    cntm.set(inet_taint, 2);

	netd_server_init(start_env->shared_container,
			 inet_taint, &cntm, &clear, h);
	netd_server_enable();
	
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
