extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
#include <inc/lib.h>

#include <inc/signal.h>

#include <stdio.h>
}

char ptrace_traceme = 0;

extern "C" void
ptrace_on_signal(struct sigaction *sa, siginfo_t *si, struct sigcontext *sc)
{
    printf("ptrace_on_signal: ...\n");
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
	    set_enosys();
	    return -1;
    }
    
    //print_backtrace();
}

