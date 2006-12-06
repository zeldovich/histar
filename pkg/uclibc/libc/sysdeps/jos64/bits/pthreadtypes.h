#ifndef UCLIBC_JOS64_PTHREADTYPES_H
#define UCLIBC_JOS64_PTHREADTYPES_H

#include <inc/jthread.h>

// Simple pthread-lookalike wrapper around a jthread_mutex_t
typedef uint64_t pthread_t;
typedef struct {
    char junk0, junk1, junk2, junk3;
    jthread_mutex_t jmu;
} pthread_mutex_t;

#define __LOCK_INITIALIZER JTHREAD_LOCK_INITIALIZER

// Types we don't care about
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;

typedef int pthread_cond_t;
typedef int pthread_condattr_t;

typedef int pthread_rwlock_t;
typedef int pthread_rwlockattr_t;

typedef int pthread_key_t;
typedef int pthread_once_t;

// Prototypes for the basic lock operations
int __pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
int __pthread_mutex_lock(pthread_mutex_t *mutex);
int __pthread_mutex_trylock(pthread_mutex_t *mutex);
int __pthread_mutex_unlock(pthread_mutex_t *mutex);

#endif
