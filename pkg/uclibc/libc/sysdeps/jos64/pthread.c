#include <pthread.h>
#include <errno.h>

int
__pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    jthread_mutex_init(&mutex->jmu);
    return 0;
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
    jthread_mutex_lock(&mutex->jmu);
    return 0;
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    int r = jthread_mutex_trylock(&mutex->jmu);
    if (r == 0)
	return 0;

    __set_errno(EBUSY);
    return -1;
}

int
__pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    jthread_mutex_unlock(&mutex->jmu);
    return 0;
}
