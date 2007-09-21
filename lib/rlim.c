#include <errno.h>
#include <string.h>
#include <sys/resource.h>

#include <bits/unimpl.h>

int
getrlimit(__rlimit_resource_t resource, struct rlimit *rlim)
{
    rlim->rlim_cur = RLIM_INFINITY;
    rlim->rlim_max = RLIM_INFINITY;
    return 0;
}

int
getrusage(int who, struct rusage *usage)
{
    memset(usage, 0, sizeof(*usage));
    return 0;
}

int
setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim)
{
    return 0;
}
