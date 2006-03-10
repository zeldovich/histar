#include <errno.h>
#include <sys/resource.h>

#include <bits/unimpl.h>

int
getrlimit(int resource, struct rlimit *rlim)
{
    set_enosys();
    return -1;
}

int
getrusage(int who, struct rusage *usage)
{
    set_enosys();
    return -1;
}

int
setrlimit(int resource, const struct rlimit *rlim)
{
    set_enosys();
    return -1;
}
