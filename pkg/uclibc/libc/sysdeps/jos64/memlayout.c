#include <inc/memlayout.h>
#include <unistd.h>

int
getpagesize(void)
{
    return PGSIZE;
}
