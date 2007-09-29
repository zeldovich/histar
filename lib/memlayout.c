#include <inc/memlayout.h>
#include <unistd.h>

libc_hidden_proto(getpagesize)

int
getpagesize(void)
{
    return PGSIZE;
}

libc_hidden_def(getpagesize)
