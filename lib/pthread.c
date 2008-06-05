#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/container.h>
#include <inc/time.h>
#include <bits/libc-tsd.h>
#include <bits/sigthread.h>

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

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    return 0;
}

int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    return 0;
}

int
pthread_setcancelstate(int state, int *oldstate)
{
    return 0;
}

int
pthread_setcanceltype(int type, int *oldtype)
{
    return 0;
}

void
pthread_testcancel(void)
{
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

void
_pthread_cleanup_push(struct _pthread_cleanup_buffer *buffer,
		      void (*routine)(void*), void *arg)
{
    buffer->__routine = routine;
    buffer->__arg = arg;
}

void
_pthread_cleanup_pop(struct _pthread_cleanup_buffer *buffer,
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
pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    return 0;
}

int
pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
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
    return pthread_cond_timedwait(cond, mu, 0);
}

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mu,
		       const struct timespec *abstime)
{
    uint64_t end_nsec = UINT64(~0);
    if (abstime)
	end_nsec = ((uint64_t) abstime->tv_sec) * NSEC_PER_SECOND +
		   abstime->tv_nsec - jos_time_nsec_offset();

    uint64_t v = cond->counter;
    pthread_mutex_unlock(mu);
    sys_sync_wait(&cond->counter, v, end_nsec);
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

int
__pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    static jthread_mutex_t once_mu;

    jthread_mutex_lock(&once_mu);
    if (*once_control) {
	jthread_mutex_unlock(&once_mu);
	return 0;
    }

    *once_control = 1;
    jthread_mutex_unlock(&once_mu);

    init_routine();
    return 0;
}

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
    cprintf("%s: unimplemented\n", __func__);
    return 0;
}

void *
pthread_getspecific(pthread_key_t key)
{
    cprintf("%s: unimplemented\n", __func__);
    return 0;
}

int
pthread_setspecific(pthread_key_t key, const void *value)
{
    cprintf("%s: unimplemented\n", __func__);
    return 0;
}

int
pthread_detach(pthread_t thread)
{
    return 0;
}

void
pthread_exit(void *value_ptr)
{
    cprintf("%s: unimplemented\n", __func__);

    for (;;)
	;
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
    cprintf("%s: unimplemented\n", __func__);
    return sigprocmask(how, set, oset);
}

int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    cprintf("%s: unimplemented\n", __func__);
    return 0;
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    cprintf("%s: unimplemented\n", __func__);
    return 0;
}

weak_alias (__pthread_mutex_init, pthread_mutex_init)
weak_alias (__pthread_mutex_lock, pthread_mutex_lock)
weak_alias (__pthread_mutex_trylock, pthread_mutex_trylock)
weak_alias (__pthread_mutex_unlock, pthread_mutex_unlock)
weak_alias (__pthread_once, pthread_once)

void *(*__libc_internal_tsd_get) (enum __libc_tsd_key_t);
int (*__libc_internal_tsd_set) (enum __libc_tsd_key_t, __const void *);
void **(*const __libc_internal_tsd_address) (enum __libc_tsd_key_t)
     __attribute__ ((__const__));

