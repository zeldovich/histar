#ifndef JOS_INC_SIGNAL_H
#define JOS_INC_SIGNAL_H

typedef int sig_atomic_t;
typedef void (*sig_t) (int);
typedef uint64_t sigset_t;
#define NSIG 32

#endif
