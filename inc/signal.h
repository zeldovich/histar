#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <signal.h>

int  kill_siginfo(pid_t pid, siginfo_t *si);

#endif
