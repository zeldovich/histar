extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/signal.h>
#include <inc/debug_gate.h>

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
    switch (request) {
        case PTRACE_TRACEME:
	    ptrace_traceme = 1;
	    return 0;
        default:
	    print_backtrace();
	    set_enosys();
	    return -1;
    }
}

