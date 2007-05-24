#ifndef UCLIBC_JOS64_UNIMPL_H
#define UCLIBC_JOS64_UNIMPL_H

#include <inc/stdio.h>
#include <errno.h>

static __inline__ void
__set_enosys(const char *func)
{
    __set_errno(ENOSYS);
    cprintf("Unimplemented function called: %s\n", func);
}

#define set_enosys()	__set_enosys(__func__)

#endif
