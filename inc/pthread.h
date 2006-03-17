#ifndef JOS_INC_PTHREAD_H
#define JOS_INC_PTHREAD_H

#include <inc/atomic.h>
#include <inc/syscall.h>
#include <inc/assert.h>

typedef atomic64_t pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER   ATOMIC_INIT(0)

static __inline __attribute__((always_inline)) void
pthread_mutex_init(pthread_mutex_t *mu, void *attr)
{
    atomic_set(mu, 0);
}

static __inline __attribute__((always_inline)) int
pthread_mutex_lock(pthread_mutex_t *mu)
{
    for (;;) {
	uint64_t cur = atomic_compare_exchange64(mu, 0, 1);
	if (cur == 0)
	    break;

	atomic_compare_exchange64(mu, 1, 2);
	sys_sync_wait(&mu->counter, cur, ~0UL);
    }
    return 0;
}

static __inline __attribute__((always_inline)) int
pthread_mutex_unlock(pthread_mutex_t *mu)
{
    uint64_t was = atomic_compare_exchange64(mu, 1, 0);
    if (was == 0)
	panic("pthread_mutex_unlock: %p not locked", mu);

    if (was == 2) {
	atomic_set(mu, 0);
	sys_sync_wakeup(&mu->counter);
    }

    return 0;
}

#endif
