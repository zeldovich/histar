extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <bits/signalgate.h>
}

#include <inc/gateparam.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static gatesrv *gs;

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
    if (gs) {
	try {
	    delete gs;
	} catch (...) {}

	gs = 0;
    }
}

void
signal_gate_init(void)
{
    signal_gate_close();

    try {
	label tl, tc;
	thread_cur_label(&tl);
	thread_cur_clearance(&tc);
	gs = new gatesrv(start_env->shared_container, "signal", &tl, &tc);
	gs->set_entry_container(start_env->proc_container);
	gs->set_entry_function(&signal_gate_entry, 0);
	gs->enable();
    } catch (std::exception &e) {
	cprintf("signal_gate_create: %s\n", e.what());
    }
}

extern "C" int
signal_gate_send(struct cobj_ref gate, siginfo_t *si)
{
    struct gate_call_data gcd;
    memset(&gcd, 0, sizeof(gcd));

    siginfo_t *sip = (siginfo_t *) &gcd.param_buf[0];
    memcpy(sip, si, sizeof(*sip));

    try {
	gate_call(gate, &gcd, 0, 0, 0);
    } catch (std::exception &e) {
	cprintf("kill: gate_call: %s\n", e.what());
	__set_errno(EPERM);
	return -1;
    }

    return 0;
}
