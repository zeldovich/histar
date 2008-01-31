#include <inc/stdio.h>
#include <semaphore.h>
#include <errno.h>

/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
int
sem_init(sem_t *sem, int pshared, unsigned int value)
{
    cprintf("sem_init: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Free resources associated with semaphore object SEM.  */
int sem_destroy(sem_t *sem)
{
    cprintf("sem_destroy: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Open a named semaphore NAME with open flaot OFLAG.  */
sem_t *sem_open(const char *name, int oflag, ...)
{
    cprintf("sem_open: Not implemented\n");
    errno = ENOSYS;
    return (sem_t *) 0;
}

/* Close descriptor for named semaphore SEM.  */
int sem_close(sem_t *sem)
{
    cprintf("sem_close: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Remove named semaphore NAME.  */
int sem_unlink(const char *name)
{
    cprintf("sem_unlink: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Wait for SEM being posted.  */
int sem_wait(sem_t *sem)
{
    cprintf("sem_wait: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Test whether SEM is posted.  */
int sem_trywait(sem_t *sem)
{
    cprintf("sem_trywait: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Post SEM.  */
int sem_post(sem_t *sem)
{
    cprintf("sem_post: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

/* Get current value of SEM and store it in *SVAL.  */
int sem_getvalue(sem_t *sem, int *sval)
{
    cprintf("sem_getvalue: Not implemented\n");
    errno = ENOSYS;
    return -1;
}

