extern "C" {
#include <bits/unimpl.h>
#include <sys/ptrace.h>
}

long int
ptrace(enum __ptrace_request request, ...) __THROW
{
    set_enosys();
    return -1;
}

