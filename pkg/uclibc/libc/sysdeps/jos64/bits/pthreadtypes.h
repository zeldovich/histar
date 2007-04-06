#ifndef UCLIBC_JOS64_PTHREADTYPES_H
#define UCLIBC_JOS64_PTHREADTYPES_H

#include <inc/jthread.h>
#include <inc/container.h>

// Simple pthread-lookalike wrapper around a jthread_mutex_t
typedef struct cobj_ref pthread_t;
typedef struct {
    int reserved;
    int count;
    uint64_t owner;
    int kind;
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
int __pthread_mutex_init(pthread_mutex_t *mutex, __const pthread_mutexattr_t *attr) __THROW;
int __pthread_mutex_lock(pthread_mutex_t *mutex) __THROW;
int __pthread_mutex_trylock(pthread_mutex_t *mutex) __THROW;
int __pthread_mutex_unlock(pthread_mutex_t *mutex) __THROW;

struct _pthread_fastlock {
    jthread_mutex_t jmu;
};

#endif
