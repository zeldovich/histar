#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;
typedef void (*sig_t) (int);
typedef uint64_t sigset_t;

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGTERM 15

#define NSIG 32

int  killpg(int pgrp, int sig);
int  kill(pid_t pid, int sig);

int  sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/* Values for the HOW argument to `sigprocmask'.  */
#define SIG_BLOCK     0          /* Block signals.  */
#define SIG_UNBLOCK   1          /* Unblock signals.  */
#define SIG_SETMASK   2          /* Set the set of blocked signals.  */

#endif
