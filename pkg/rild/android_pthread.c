extern "C" {

/*
 * bionic -> HiStar pthread conversions (we renamed symbols in the binary).
 */

enum { Xthread_debug_cond = 0 };
enum { Xthread_debug_mutex = 0 };
enum { Xthread_debug_thread = 0 };

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <inc/assert.h>
#include <inc/lib.h>
#include <inc/time.h>


/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
typedef long Xthread_t;
typedef long Xthread_mutexattr_t;
typedef long Xthread_condattr_t;

typedef struct
{
    int volatile value;
} Xthread_cond_t;

typedef struct
{
    int volatile value;
} Xthread_mutex_t;

typedef struct
{
    uint32_t flags;
    void * stack_base;
    size_t stack_size;
    size_t guard_size;
    int32_t sched_policy;
    int32_t sched_priority;
} Xthread_attr_t;

#define NLOOKUPS	256	/* yes, the whole thing is lame */

static struct {
	Xthread_mutex_t *xt;
	pthread_mutex_t  pt;
} mutex_lookup[NLOOKUPS];

static struct {
	Xthread_cond_t *xt;
	pthread_cond_t  pt;
} cond_lookup[NLOOKUPS];

static pthread_mutex_t *
mutex_get(Xthread_mutex_t *xt)
{
	int i;

	for (i = 0; i < NLOOKUPS; i++) {
		if (mutex_lookup[i].xt == xt)
			return (&mutex_lookup[i].pt);
	}

	// there's no corresponding pthread mutex, so create one if we can
	int slot = -1;
	for (i = 0; i < NLOOKUPS; i++) {
		if (mutex_lookup[i].xt == NULL) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		fprintf(stderr, "FATAL ERROR: OUT OF MUTEX LOOKUP INDICIES!\n");
		exit(1);
	}

	if (pthread_mutex_init(&mutex_lookup[slot].pt, NULL)) {
		fprintf(stderr, "FATAL ERROR: PTHREAD_MUTEX_INIT FAILED\n");
		exit(1);
	}
	mutex_lookup[slot].xt = xt;
	return (&mutex_lookup[slot].pt);
}

static pthread_cond_t *
cond_get(Xthread_cond_t *xt)
{
	int i;

	for (i = 0; i < NLOOKUPS; i++) {
		if (cond_lookup[i].xt == xt)
			return (&cond_lookup[i].pt);
	}

	// there's no corresponding pthread cond, so create one if we can
	int slot = -1;
	for (i = 0; i < NLOOKUPS; i++) {
		if (cond_lookup[i].xt == NULL) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		fprintf(stderr, "FATAL ERROR: OUT OF COND LOOKUP INDICIES!\n");
		exit(1);
	}

	if (pthread_cond_init(&cond_lookup[slot].pt, NULL)) {
		fprintf(stderr, "FATAL ERROR: PTHREAD_COND_INIT FAILED\n");
		exit(1);
	}
	cond_lookup[slot].xt = xt;
	return (&cond_lookup[slot].pt);
}

int Xthread_create(Xthread_t *, Xthread_attr_t const *,
		    void *(*)(void *), void *);
int Xthread_mutex_init(Xthread_mutex_t *,
		    const Xthread_mutexattr_t *);
int Xthread_cond_wait(Xthread_cond_t *, Xthread_mutex_t *);
int Xthread_mutex_destroy(Xthread_mutex_t *);
int Xthread_mutex_lock(Xthread_mutex_t *);
int Xthread_mutex_unlock(Xthread_mutex_t *);
int Xthread_cond_signal(Xthread_cond_t *);
int Xthread_cond_timedwait(Xthread_cond_t *,
		    Xthread_mutex_t *, const struct timespec *);
int Xthread_cond_timeout_np(Xthread_cond_t *, Xthread_mutex_t *,
		    unsigned);

int
Xthread_create(Xthread_t *thread, Xthread_attr_t const *attr,
    void *(*entry)(void *), void *arg)
{
	if (Xthread_debug_thread)
		fprintf(stderr, "%s: tid % " PRIx64 ", thread %p, thread_attr %p, entry %p, arg %p, (from %p)\n", __func__, thread_id(), thread, attr, entry, arg, __builtin_return_address(0));

	pthread_t t;
	int ret;

	// libhtc_ril.so never refers to the tid, so let its pthread_t dangle
	ret = pthread_create(&t, NULL, entry, arg);
	if (ret != 0)
		*thread = (long)t;

	return (ret);
}

int
Xthread_join(Xthread_t thread, void **value_ptr)
{
	return (pthread_join(thread, value_ptr));
}

int
Xthread_mutex_init(Xthread_mutex_t *mutex, const Xthread_mutexattr_t *attr)
{
	if (Xthread_debug_mutex)
		fprintf(stderr, "%s: tid %" PRIx64 ", mutex %p, mutexattr %p (frrom %p)\n", __func__, thread_id(), mutex, attr, __builtin_return_address(0));

	// mutex_get should create the mutex
	mutex_get(mutex);
	return (0);
}

int
Xthread_cond_wait(Xthread_cond_t *cond, Xthread_mutex_t *mutex)
{
	if (Xthread_debug_cond)
		fprintf(stderr, "%s: tid %" PRIx64 ", cond %p, mutex %p (from %p)\n", __func__, thread_id(), cond, mutex, __builtin_return_address(0));

	pthread_cond_t *ct = cond_get(cond);
	pthread_mutex_t *mt = mutex_get(mutex);
	return (pthread_cond_wait(ct, mt));
}

int
Xthread_mutex_destroy(Xthread_mutex_t *mutex)
{
	int i;

	if (Xthread_debug_mutex)
		fprintf(stderr, "%s: tid %" PRIx64 ", mutex %p (from %p)\n", __func__, thread_id(), mutex,
		    __builtin_return_address(0));

	pthread_mutex_t *pt = mutex_get(mutex);
	for (i = 0; i < NLOOKUPS; i++) {
		if (mutex_lookup[i].xt == mutex) {
			mutex_lookup[i].xt = NULL;
			break;
		}
	}
	return (pthread_mutex_destroy(pt));
}

int
Xthread_mutex_lock(Xthread_mutex_t *mutex)
{
	if (Xthread_debug_mutex)
		fprintf(stderr, "%s: tid %" PRIx64 ", mutex %p (from %p)\n", __func__, thread_id(), mutex,
		    __builtin_return_address(0));

	pthread_mutex_t *pt = mutex_get(mutex);
	return (pthread_mutex_lock(pt));
}

int
Xthread_mutex_unlock(Xthread_mutex_t *mutex)
{
	if (Xthread_debug_mutex)
		fprintf(stderr, "%s: tid %" PRIx64 ", mutex %p (from %p)\n", __func__, thread_id(), mutex,
		    __builtin_return_address(0));

	pthread_mutex_t *pt = mutex_get(mutex);
	return (pthread_mutex_unlock(pt));
}

int
Xthread_cond_signal(Xthread_cond_t *cond)
{
	if (Xthread_debug_cond)
		fprintf(stderr, "%s: tid %" PRIx64 ", cond %p (from %p)\n", __func__, thread_id(), cond,
		    __builtin_return_address(0));

	pthread_cond_t *ct = cond_get(cond);
	return (pthread_cond_signal(ct));
}

int
Xthread_cond_timedwait(Xthread_cond_t *cond,
    Xthread_mutex_t * mutex, const struct timespec *abstime)
{
	if (Xthread_debug_cond)
		fprintf(stderr, "%s: tid %" PRIx64 ", cond %p, mutex %p, abstime %p (from %p)\n", __func__, thread_id(), cond, mutex, abstime, __builtin_return_address(0));

	pthread_cond_t *ct = cond_get(cond);
	pthread_mutex_t *mt = mutex_get(mutex);
	
	int ret = pthread_cond_timedwait(ct, mt, abstime);
	assert(ret == 0 || ret == 110 /* ETIMEDOUT */);
	return (ret);

}

int
pthread_cond_timeout_np(pthread_cond_t *cond, pthread_mutex_t *mutex, unsigned msecs)
{
	struct timespec abstime;

	if (clock_gettime(CLOCK_REALTIME, &abstime))
		panic("%s: clock_gettime failed", __func__);

	// NB: doesn't matter if tv_nsec exceeds NSEC_PER_SECOND
	abstime.tv_sec  += msecs / 1000;
	abstime.tv_nsec += (msecs % 1000) * 1000 * 1000;

	return (pthread_cond_timedwait(cond, mutex, &abstime));
}

// XXX- not sure if this is right...
int
Xthread_cond_timeout_np(Xthread_cond_t *cond, Xthread_mutex_t *mutex,
    unsigned msecs)
{
	if (Xthread_debug_cond)
		fprintf(stderr, "%s: tid %" PRIx64 ", cond %p, mutex %p, msecs: %u (from %p)\n", __func__, thread_id(), cond, mutex, msecs, __builtin_return_address(0));

	pthread_cond_t *ct = cond_get(cond);
	pthread_mutex_t *mt = mutex_get(mutex);

	int ret = pthread_cond_timeout_np(ct, mt, msecs);
	assert(ret == 0 || ret == 110 /* ETIMEDOUT */);
	return (ret);
}

} // extern "C"
