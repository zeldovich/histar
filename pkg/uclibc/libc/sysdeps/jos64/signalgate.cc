extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <inc/gateparam.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <bits/signalgate.h>
}

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static struct cobj_ref gs;

static void __attribute__ ((noreturn))
signal_gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    siginfo_t *si = (siginfo_t *) &gcd->param_buf[0];
    static_assert(sizeof(*si) <= sizeof(gcd->param_buf));

    signal_process_remote(si);
    gr->ret(0, 0, 0);
}

void
signal_gate_close(void)
{
    if (gs.object) {
	sys_obj_unref(gs);
	gs.object = 0;
    }
}

void
signal_gate_init(void)
{
    signal_gate_close();

    try {
	label tl, tc;
	// avoid calling functions that manipulate label cache
	get_label_retry(&tl, thread_get_label);
	get_label_retry(&tc, sys_self_get_clearance);

	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.name_ = "signal";
	gd.label_ = &tl;
	gd.clearance_ = &tc;
	gd.func_ = &signal_gate_entry;
	gd.arg_ = 0;
	gd.flags_ = GATESRV_KEEP_TLS_STACK;
	gs = gate_create(&gd);
    } catch (std::exception &e) {
	cprintf("signal_gate_create: %s\n", e.what());
    }
}

extern "C" int
signal_gate_send(struct cobj_ref gate, siginfo_t *si)
{
    struct gate_call_data gcd;
    siginfo_t *sip = (siginfo_t *) &gcd.param_buf[0];
    memcpy(sip, si, sizeof(*sip));

    try {
	gate_call(gate, 0, 0, 0).call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("kill: gate_call: %s\n", e.what());
	__set_errno(EPERM);
	return -1;
    }

    return 0;
}
