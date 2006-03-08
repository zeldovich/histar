#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;
typedef void (*sig_t) (int);
typedef uint64_t sigset_t;

#define SIGTERM 0
#define NSIG 32

int  killpg(int pgrp, int sig);
int  kill(pid_t pid, int sig);

#endif
