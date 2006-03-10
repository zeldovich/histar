#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>

#include <bits/unimpl.h>

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
    set_enosys();
    return -1;
}

pid_t
getpgid(pid_t pid)
{
    set_enosys();
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

pid_t
waitpid(pid_t pid, int *wait_stat, int options)
{
    set_enosys();
    return -1;
}

pid_t
wait4(pid_t pid, int *status, int options, struct rusage *rusage)
{
    set_enosys();
    return -1;
}

int
fork(void)
{
    set_enosys();
    return -1;
}

int
execve(const char * filename, char *const * argv, char *const * envp)
{
    set_enosys();
    return -1;
}
