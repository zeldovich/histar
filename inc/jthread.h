#ifndef JOS_INC_JTHREAD_H
#define JOS_INC_JTHREAD_H

#include <inc/atomic.h>

typedef jos_atomic64_t jthread_mutex_t;
#define JTHREAD_LOCK_INITIALIZER	{ 0 }

void jthread_mutex_init(jthread_mutex_t *mu);
void jthread_mutex_lock(jthread_mutex_t *mu);
int  jthread_mutex_trylock(jthread_mutex_t *mu);
void jthread_mutex_unlock(jthread_mutex_t *mu);

#endif
