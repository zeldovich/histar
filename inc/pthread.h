#ifndef JOS_INC_PTHREAD_H
#define JOS_INC_PTHREAD_H

#include <inc/atomic.h>
#include <inc/syscall.h>

typedef atomic_t pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER   ATOMIC_INIT(0)

static __inline __attribute__((always_inline)) void
pthread_mutex_init(pthread_mutex_t *mu, void *attr)
{
    atomic_set(mu, 0);
}

static __inline __attribute__((always_inline)) int
pthread_mutex_lock(pthread_mutex_t *mu)
{
    while (atomic_compare_exchange(mu, 0, 1) != 0)
	sys_thread_yield();
    return 0;
}

static __inline __attribute__((always_inline)) int
pthread_mutex_unlock(pthread_mutex_t *mu)
{
    atomic_set(mu, 0);
    return 0;
}

#endif
