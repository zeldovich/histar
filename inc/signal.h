#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <inc/container.h>
#include <signal.h>

int  kill_siginfo(pid_t pid, siginfo_t *si);
int  kill_thread_siginfo(struct cobj_ref tid, siginfo_t *si);
void segfault_helper(siginfo_t *si, struct sigcontext *sc);
void signal_trap_if_pending(void);

#endif
