#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/semaphore.h>
#include <errno.h>

/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
int
sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (pshared) {
        errno = ENOSYS;
        return -1;
    }

    jthread_mutex_init(&sem->sem_lock);
    sem->sem_value = value;
    return 0;
}

/* Free resources associated with semaphore object SEM.  */
int
sem_destroy(sem_t *sem)
{
    return 0;
}

/* Wait for SEM being posted.  */
int
sem_wait(sem_t *sem)
{
    while (1) {
        jthread_mutex_lock(&sem->sem_lock);
        if (sem->sem_value > 0) {
            sem->sem_value--;
            jthread_mutex_unlock(&sem->sem_lock);
            return 0;
        }
        jthread_mutex_unlock(&sem->sem_lock);
        /* Go to sleep and try to reacquire when woken */
        sys_sync_wait(&sem->sem_value, 0, UINT64(~0));
    }
}

/* Test whether SEM is posted.  */
int
sem_trywait(sem_t *sem)
{
    int r;

    jthread_mutex_lock(&sem->sem_lock);
    if (sem->sem_value == 0) {
        errno = EAGAIN;
        r = -1;
    } else {
        sem->sem_value--;
        r = 0;
    }
    jthread_mutex_unlock(&sem->sem_lock);

    return r;
}

/* Post SEM.  */
int
sem_post(sem_t *sem)
{
    jthread_mutex_lock(&sem->sem_lock);
    if (sem->sem_value >= SEM_VALUE_MAX) {
        errno = ERANGE;
        jthread_mutex_unlock(&sem->sem_lock);
        return -1;
    }
    sem->sem_value++;
    if (sem->sem_value == 1)
        sys_sync_wakeup(&sem->sem_value);
    jthread_mutex_unlock(&sem->sem_lock);
    return 0;
}

/* Get current value of SEM and store it in *SVAL.  */
int
sem_getvalue(sem_t *sem, int *sval)
{
    *sval = sem->sem_value;
    return 0;
}

/* Open a named semaphore NAME with open flaot OFLAG.  */
sem_t *
sem_open(const char *name, int oflag, ...)
{
    cprintf("sem_open: Not implemented\n");
    errno = ENOSYS;
    return (sem_t *) 0;
}

/* Close descriptor for named semaphore SEM.  */
int
sem_close(sem_t *sem)
{
    cprintf("sem_close: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Remove named semaphore NAME.  */
int
sem_unlink(const char *name)
{
    cprintf("sem_unlink: Not implemented\n");
    errno = ENOSYS;
    return -1;
}
