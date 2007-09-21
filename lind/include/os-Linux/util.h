#ifndef LINUX_ARCH_INCLUDE_OS_LINUX_UTIL_H
#define LINUX_ARCH_INCLUDE_OS_LINUX_UTIL_H

#include <sys/syscall.h>

/* Don't use the glibc version, which caches the result in TLS. It misses some
 * syscalls, and also breaks with clone(), which does not unshare the TLS.
 */
long inline
util_getpid(void) 
{
    return(syscall(__NR_getpid));
}

#endif
