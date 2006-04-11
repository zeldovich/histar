#ifndef JOS_INC_PTHREAD_H
#define JOS_INC_PTHREAD_H

#include <inc/atomic.h>
#include <inc/assert.h>

typedef atomic64_t pthread_mutex_t;

void pthread_mutex_init(pthread_mutex_t *mu, void *attr);
int  pthread_mutex_lock(pthread_mutex_t *mu);
int  pthread_mutex_unlock(pthread_mutex_t *mu);

#endif
