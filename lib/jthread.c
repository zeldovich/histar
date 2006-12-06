#include <inc/jthread.h>
#include <inc/syscall.h>
#include <inc/assert.h>

void
jthread_mutex_init(jthread_mutex_t *mu)
{
    atomic_set(mu, 0);
}

int
jthread_mutex_lock(jthread_mutex_t *mu)
{
    for (;;) {
	uint64_t cur = atomic_compare_exchange64(mu, 0, 1);
	if (cur == 0)
	    break;

	atomic_compare_exchange64(mu, 1, 2);
	sys_sync_wait(&mu->counter, 2, ~0UL);
    }
    return 0;
}

int
jthread_mutex_trylock(jthread_mutex_t *mu)
{
    uint64_t cur = atomic_compare_exchange64(mu, 0, 1);
    if (cur == 0)
	return 0;

    return -1;
}

int
jthread_mutex_unlock(jthread_mutex_t *mu)
{
    uint64_t was = atomic_compare_exchange64(mu, 1, 0);
    if (was == 0)
	panic("jthread_mutex_unlock: %p not locked", mu);

    if (was == 2) {
	atomic_set(mu, 0);
	sys_sync_wakeup(&mu->counter);
    }

    return 0;
}
