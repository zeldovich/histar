#ifndef _JOS_INC_PTRACE_H
#define _JOS_INC_PTRACE_H

#include <inc/signal.h>

extern char ptrace_traceme;

void ptrace_on_signal(struct sigaction *sa, siginfo_t *si, struct sigcontext *sc);

#endif
