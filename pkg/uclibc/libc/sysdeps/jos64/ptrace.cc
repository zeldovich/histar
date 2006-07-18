extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/debug_gate.h>

#include <stdarg.h>
#include <stdio.h>
}

char ptrace_traceme = 0;

extern "C" void
ptrace_on_signal(struct sigaction *sa, siginfo_t *si, struct sigcontext *sc)
{
    debug_gate_signal_stop(si->si_signo);
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
    
    switch (request) {
        case PTRACE_TRACEME:
	    ptrace_traceme = 1;
	    return 0;
        case PTRACE_GETREGS:
        default:
	    cprintf("ptrace: unknown request %d\n", request);
	    print_backtrace();
	    set_enosys();
	    return -1;
    }
}

