#include <inc/atomic.h>
#include <inc/syscall.h>

typedef atomic_t mutex_t;

static __inline __attribute__((always_inline)) void
mutex_lock(mutex_t *mu)
{
    while (atomic_compare_exchange(mu, 0, 1) != 0)
	sys_thread_yield();
}

static __inline __attribute__((always_inline)) void
mutex_unlock(mutex_t *mu)
{
    atomic_set(mu, 0);
}
