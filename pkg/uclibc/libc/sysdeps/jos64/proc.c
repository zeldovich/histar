#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>

pid_t
getpid()
{
    return thread_id();
}

pid_t
getppid()
{
    return 0;
}

int
nice(int n)
{
    return 0;
}

int
setpgid(pid_t pid, pid_t pgid)
{
    __set_errno(ENOSYS);
    return -1;
}

pid_t
getpgid(pid_t pid)
{
    __set_errno(ENOSYS);
    return -1;
}

int
setpgrp(void)
{
    return setpgid(0, 0);
}

pid_t
getpgrp(void)
{
    return getpgid(0);
}
