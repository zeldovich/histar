#include <inc/rand.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>
#include <inc/memlayout.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <stdio.h>


int
rand_open(int flags)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "rand");
    if (r < 0) {
    	errno = ENOMEM;
    	return -1;
    }

    fd->fd_dev_id = devrand.dev_id;
    fd->fd_omode = flags;

    return fd2num(fd);
}

static ssize_t
rand_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    uint64_t rsp;
    __asm __volatile("movq %%rsp,%0" : "=r" (rsp));
    char *sp = (char *)(ROUNDUP(rsp, PGSIZE) - 1);
    
    uint64_t count = 0;
    for (; count < (PGSIZE - 1) && count < len; count++, sp--)
        ((char *)buf)[count] = (*sp + count) * count;    
            
    return count;    
}

static ssize_t
rand_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    return 0;
}

static int
rand_probe(struct Fd *fd, dev_probe_t probe)
{
    return 1;
}

static int
rand_close(struct Fd *fd)
{
    return 0;
}

struct Dev devrand = {
    .dev_id = 'r',
    .dev_name = "rand",
    .dev_read = &rand_read,
    .dev_write = &rand_write,
    .dev_probe = &rand_probe,
    .dev_close = &rand_close,
};
