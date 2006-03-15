#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <bits/unimpl.h>

pid_t
getpid()
{
    return start_env->shared_container;
}

pid_t
getppid()
{
    return start_env->ppid;
}

int
nice(int n)
{
    return 0;
}

int
setpgid(pid_t pid, pid_t pgid)
{
    set_enosys();
    return -1;
}

pid_t
__getpgid(pid_t pid)
{
    set_enosys();
    return -1;
}

pid_t
getpgrp(void)
{
    return __getpgid(0);
}
