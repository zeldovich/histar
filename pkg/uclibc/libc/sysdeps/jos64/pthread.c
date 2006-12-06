#include <pthread.h>
#include <errno.h>
#include <inc/lib.h>
#include <inc/assert.h>

int
__pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
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
