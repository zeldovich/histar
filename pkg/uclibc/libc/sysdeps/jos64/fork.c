#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>

#include <bits/unimpl.h>

int
fork(void)
{
    set_enosys();
    return -1;
}
