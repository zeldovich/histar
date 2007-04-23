#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/container.h>

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

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return 0;
}

void
_pthread_cleanup_push_defer(struct _pthread_cleanup_buffer *buffer,
			    void (*routine)(void *), void *arg)
{
    buffer->__routine = routine;
    buffer->__arg = arg;
}

void
_pthread_cleanup_pop_restore(struct _pthread_cleanup_buffer *buffer,
			     int execute)
{
    if (execute)
	buffer->__routine(buffer->__arg);
}

int
pthread_create(pthread_t *__restrict tid,
	       __const pthread_attr_t *__restrict attr,
	       void *(*startfn) (void *),
	       void *__restrict arg)
{
    struct cobj_ref cobj_tid;
    void (*startfn_void) (void *) = (void *) startfn;
    int r = thread_create(start_env->proc_container,
			  startfn_void, arg,
			  &cobj_tid, "pthread");
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    *tid = cobj_tid.object;
    return 0;
}

int
pthread_join(pthread_t tid, void **retp)
{
    __set_errno(ENOSYS);
    return -1;
}

pthread_t
pthread_self(void)
{
    return thread_id();
}

int
pthread_attr_init(pthread_attr_t *attr)
{
    return 0;
}

int
pthread_attr_destroy(pthread_attr_t *attr)
{
    return 0;
}

int
pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
    return 0;
}

int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    cond->counter = 0;
    return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cond)
{
    return 0;
}

int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mu)
{
    uint64_t v = cond->counter;
    pthread_mutex_unlock(mu);
    sys_sync_wait(&cond->counter, v, ~0UL);
    pthread_mutex_lock(mu);
    return 0;
}

int
pthread_cond_signal(pthread_cond_t *cond)
{
    return pthread_cond_broadcast(cond);
}

int
pthread_cond_broadcast(pthread_cond_t *cond)
{
    cond->counter++;
    sys_sync_wakeup(&cond->counter);
    return 0;
}

int
pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

weak_alias (__pthread_mutex_init, pthread_mutex_init)
weak_alias (__pthread_mutex_lock, pthread_mutex_lock)
weak_alias (__pthread_mutex_trylock, pthread_mutex_trylock)
weak_alias (__pthread_mutex_unlock, pthread_mutex_unlock)

