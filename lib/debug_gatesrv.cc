extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/debug_gate.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <bits/signalgate.h>
}

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static struct cobj_ref gs;

static void __attribute__ ((noreturn))
debug_gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    struct debug_args *da = (struct debug_args *) &gcd->param_buf[0];
    static_assert(sizeof(*da) <= sizeof(gcd->param_buf));
    
    printf("hello from debug_gate_entry\n");
    
    gr->ret(0, 0, 0);
}

void
debug_gate_close(void)
{
    if (gs.object) {
	sys_obj_unref(gs);
	gs.object = 0;
    }
}

void
debug_gate_init(void)
{
    debug_gate_close();

    try {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);

	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.name_ = "debug";
	gd.label_ = &tl;
	gd.clearance_ = &tc;
	gd.func_ = &debug_gate_entry;
	gd.arg_ = 0;
	gd.flags_ = GATESRV_KEEP_TLS_STACK;
	gs = gate_create(&gd);
    } catch (std::exception &e) {
	cprintf("signal_gate_create: %s\n", e.what());
    }
}

extern "C" int
debug_gate_send(struct cobj_ref gate, struct debug_args *da)
{
    struct gate_call_data gcd;
    struct debug_args *dag = (struct debug_args *) &gcd.param_buf[0];
    memcpy(dag, da, sizeof(*dag));

    try {
	gate_call(gate, 0, 0, 0).call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("kill: gate_call: %s\n", e.what());
	errno = EPERM;
	return -1;
    }

    return 0;
}
