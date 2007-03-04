#include <pthread.h>
#include <errno.h>
#include <inc/lib.h>
#include <inc/assert.h>

int
__pthread_mutex_init(pthread_mutex_t * mutex,
		     const pthread_mutexattr_t *attr)
{
    memset(mutex, 0, sizeof(*mutex));
    jthread_mutex_init(&mutex->jmu);
    return 0;
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex->kind == PTHREAD_MUTEX_RECURSIVE) {
	uint64_t tid = thread_id();
	if (mutex->owner == tid) {
	    mutex->count++;
	    return 0;
	}

	jthread_mutex_lock(&mutex->jmu);
	assert(mutex->count == 0);
	assert(mutex->owner == 0);
	mutex->count = 1;
	mutex->owner = tid;
    } else {
	jthread_mutex_lock(&mutex->jmu);
    }
    return 0;
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    if (mutex->kind == PTHREAD_MUTEX_RECURSIVE) {
	uint64_t tid = thread_id();
	if (mutex->owner == tid) {
	    mutex->count++;
	    return 0;
	}

	if (0 == jthread_mutex_trylock(&mutex->jmu)) {
	    assert(mutex->count == 0);
	    assert(mutex->owner == 0);
	    mutex->count = 1;
	    mutex->owner = tid;
	    return 0;
	}
    } else {
	if (0 == jthread_mutex_trylock(&mutex->jmu))
	    return 0;
    }

    __set_errno(EBUSY);
    return -1;
}

int
__pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (mutex->kind == PTHREAD_MUTEX_RECURSIVE) {
	assert(mutex->owner == thread_id());
	assert(mutex->count != 0);
	mutex->count--;

	if (mutex->count == 0) {
	    mutex->owner = 0;
	    jthread_mutex_unlock(&mutex->jmu);
	}

	return 0;
    } else {
	jthread_mutex_unlock(&mutex->jmu);
    }

    return 0;
}

weak_alias (__pthread_mutex_init, pthread_mutex_init)
weak_alias (__pthread_mutex_lock, pthread_mutex_lock)
weak_alias (__pthread_mutex_trylock, pthread_mutex_trylock)
weak_alias (__pthread_mutex_unlock, pthread_mutex_unlock)

