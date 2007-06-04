extern "C" {
#include <inc/lib.h>
#include <inc/netd_linux.h>
#include <inc/stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>

static void __attribute__((noreturn))
netd_entry(uint64_t arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gr->ret(0, 0, 0);
}

int
netd_linux_server_init(void (*handler)(struct netd_op_args *))
{
    try {
	label l(1);
	label c(2);

	thread_cur_label(&l);
	thread_cur_clearance(&c);
	
	uint64_t ct = start_env->shared_container;
	gate_create(ct, "netd", &l, &c, 0, &netd_entry, (uintptr_t)handler);
    } catch (std::exception &e) {
	cprintf("netd_linux_server_init: error: %s\n", e.what());
	return -1;
    }
    return 0;
}
