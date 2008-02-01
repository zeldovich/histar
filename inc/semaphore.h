#ifndef JOS_INC_SEMAPHORE_H
#define JOS_INC_SEMAPHORE_H

#include <inc/jthread.h>

/* Maximum value the semaphore can have.  */
#define SEM_VALUE_MAX   (2147483647)

typedef struct
{
    jthread_mutex_t sem_lock;
    uint64_t sem_value;
} sem_t;

/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
int sem_init(sem_t *sem, int pshared, unsigned int value);

/* Free resources associated with semaphore object SEM.  */
int sem_destroy(sem_t *sem);

/* Open a named semaphore NAME with open flaot OFLAG.  */
sem_t *sem_open(const char *name, int oflag, ...);

/* Close descriptor for named semaphore SEM.  */
int sem_close(sem_t *sem);

/* Remove named semaphore NAME.  */
int sem_unlink(const char *name);

/* Wait for SEM being posted.  */
int sem_wait(sem_t *sem);

/* Test whether SEM is posted.  */
int sem_trywait(sem_t *sem);

/* Post SEM.  */
int sem_post(sem_t *sem);

/* Get current value of SEM and store it in *SVAL.  */
int sem_getvalue(sem_t *sem, int *sval);

#endif

