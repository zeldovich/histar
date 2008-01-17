#include <inc/stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <inc/lib.h>
#include <stdio.h>
#include <inc/debug.h>


int
shmget(key_t key, size_t size, int shmflg)
{
    cprintf("shmdt: Not implemented\n");
    return -1;
}

void *
shmat(int shmid, const void *shmaddr, int shmflg)
{
    cprintf("shmdt: Not implemented\n");
    return NULL;
}

int
shmdt(const void *shmaddr)
{
    cprintf("shmdt: Not implemented\n");
    return -1;
}

int
shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    cprintf("shmctl: Not implemented\n");
    return -1;
}

