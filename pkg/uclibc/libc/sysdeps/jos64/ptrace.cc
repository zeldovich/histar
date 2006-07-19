extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/debug_gate.h>
#include <inc/debug.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/user.h>
}

#include <inc/scopeguard.hh>
#include <inc/error.hh>

static const char ptrace_dbg = 1;

char ptrace_traceme = 0;

extern "C" void
ptrace_on_signal(struct sigaction *sa, siginfo_t *si, struct sigcontext *sc)
{
    debug_gate_signal_stop(si->si_signo, sc);
    return;
}

long int
ptrace(enum __ptrace_request request, ...) __THROW
{
    pid_t pid;
    void *addr, *data;

    va_list ap;
    va_start(ap, request);
    pid = va_arg(ap, pid_t);
    addr = va_arg(ap, void*);
    data = va_arg(ap, void*);
    va_end(ap);
    
    if (request == PTRACE_TRACEME) {
	ptrace_traceme = 1;
	return 0;
    }
    
    uint64_t ct = pid;
    int64_t gate_id = container_find(ct, kobj_gate, "debug");
    if (gate_id < 0) {
	debug_print(ptrace_dbg, "couldn't find debug gate for %ld\n", pid);
	return 0;
    }
    
    struct debug_args args;
    
    debug_print(ptrace_dbg, "request %d", request);

    switch (request) {
        case PTRACE_GETREGS:
	case PTRACE_GETFPREGS: {
	    args.op = request == PTRACE_GETREGS ? da_getregs : da_getfpregs;
	    debug_gate_send(COBJ(ct, gate_id), &args);
	    if (args.ret < 0)
		return args.ret;
	    else if (args.ret == 0)
		return 0;
	    
	    struct user_regs_struct *regs = 0;
	    error_check(segment_map(args.ret_cobj, 0, SEGMAP_READ, 
				    (void **)&regs, 0, 0));
	    scope_guard<int, void*> seg_unmap(segment_unmap, regs);
	    scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, args.ret_cobj);
	    memcpy(data, regs, args.ret);
	    return 0;
	}
	case PTRACE_PEEKTEXT: {
	    args.op = da_peektext;
	    args.addr = (uint64_t)addr;
	    debug_gate_send(COBJ(ct, gate_id), &args);
	    return args.ret;
	}
        default:
	    cprintf("ptrace: unknown request %d\n", request);
	    print_backtrace();
	    set_enosys();
	    return -1;
    }
}

