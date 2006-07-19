extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/debug_gate.h>
#include <inc/debug.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <bits/signalgate.h>
#include <sys/user.h>
}

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

static const char debug_gate_enable = 1;
static const char debug_dbg = 1;

static struct cobj_ref gs;
static struct
{
    uint64_t wait;
    char     signo;
    struct user_regs_struct regs;
} ptrace_info;

static void
debug_gate_wait(struct debug_args *da)
{
    da->ret = ptrace_info.signo;
}

static void
debug_gate_getregs(struct debug_args *da)
{
    if (!ptrace_info.signo) {
	debug_print(debug_dbg, "process not stopped!?");
	da->ret = -1;
	return;
    }

    struct cobj_ref ret_seg;
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container,
			      sizeof(ptrace_info.regs),
			      &ret_seg, &va,
			      0, "regs segment"));
    scope_guard<int, void*> seg_unmap(segment_unmap, va);
    memcpy(va, &ptrace_info.regs, sizeof(ptrace_info.regs));
    da->ret_cobj = ret_seg;
    da->ret = 0;
}

static void __attribute__ ((noreturn))
debug_gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    struct debug_args *da = (struct debug_args *) &gcd->param_buf[0];
    static_assert(sizeof(*da) <= sizeof(gcd->param_buf));
    
    switch(da->op) {
        case da_wait:
	    debug_gate_wait(da);
	    break;
        case da_getregs:
	    debug_gate_getregs(da);
	    break;
        default:
	    break;
    }
    
    gr->ret(0, 0, 0);
}

void
debug_gate_close(void)
{
    memset(&ptrace_info, 0, sizeof(ptrace_info));
    if (gs.object) {
	sys_obj_unref(gs);
	gs.object = 0;
    }
}

void
debug_gate_init(void)
{
    debug_print(debug_dbg, "debug_gate_init");
    debug_gate_close();
    if (!debug_gate_enable)
	return;

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

void
debug_gate_signal_stop(char signo, struct sigcontext *sc)
{
    ptrace_info.signo = signo;
    if (!sc) 
	debug_print(debug_dbg, "null struct sigcontext");
    else {
        struct UTrapframe *u = &sc->sc_utf;
	struct user_regs_struct *r = &ptrace_info.regs;
	
	r->r15 = u->utf_r15;
	r->r14 = u->utf_r14;
	r->r13 = u->utf_r13;
	r->r12 = u->utf_r12;
	r->rbp = u->utf_rbp;
	r->rbx = u->utf_rbx;
	r->r11 = u->utf_r11;
	r->r10 = u->utf_r10;
	r->r9 = u->utf_r9;
	r->r8 = u->utf_r8;
	r->rax = u->utf_rax;
	r->rcx = u->utf_rcx;
	r->rdx = u->utf_rdx;
	r->rsi = u->utf_rsi;
	r->rdi = u->utf_rdi;
	//r->orig_rax = 
	r->rip = u->utf_rip;
	//r->cs = 
	r->eflags = u->utf_rflags;
	r->rsp = u->utf_rsp;
	//r->ss = 
	//r->fs_base = 
	//r->gs_base =
	//r->ds = 
	//r->es = 
	//r->fs =
	//r->gs = 
    }
    sys_sync_wait(&ptrace_info.wait, 0, ~0L);
}

extern "C" int64_t
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
    memcpy(da, dag, sizeof(*da));
    return da->ret;
}
