#include <inc/types.h>
#include <inc/error.h>
#include <inc/syscall_num.h>

#ifdef JOS_KERNEL
#include <kern/lib.h>
#else /* !JOS_KERNEL */
#include <inc/stdio.h>
#endif /* !JOS_KERNEL */

static const char *const error_string[E_MAXERROR + 1] = {
    [0]		    = "no error",
    [E_UNSPEC]	    = "unspecified error",
    [E_INVAL]	    = "invalid parameter",
    [E_NO_MEM]	    = "out of memory",
    [E_RESTART]	    = "restart system call",
    [E_NOT_FOUND]   = "object not found",
    [E_LABEL]	    = "label check failure",
    [E_BUSY]	    = "device busy",
    [E_NO_SPACE]    = "not enough space in buffer",
    [E_AGAIN]	    = "try again",
    [E_IO]	    = "disk IO error",
    [E_FIXED_QUOTA] = "object has fixed quota",
    [E_VAR_QUOTA]   = "object has variable quota",
    [E_RESOURCE]    = "container out of space",
    [E_RANGE]	    = "value out of range",
    [E_EOF]	    = "unexpected end-of-file",
    [E_MAX_OPEN]    = "out of file descriptors",
    [E_BAD_OP]      = "operation not supported",
    [E_EXISTS]	    = "already exists",
    [E_MAXERROR]    = "error code out of range",
};

const char *
e2s(int err) {
    if (err < 0)
	err = -err;
    if (err > E_MAXERROR)
	err = E_MAXERROR;
    const char *s = error_string[err];
    if (s == 0)
	s = "missing error definition in error_string[] table";
    return s;
}

#define SYSCALL_ENTRY(name) [SYS_##name] = #name,
static const char *const syscall_names[NSYSCALLS] = {
    ALL_SYSCALLS
};
#undef SYSCALL_ENTRY

const char *
syscall2s(int sys) {
    if (sys < 0 || sys >= NSYSCALLS)
	return "out of range";
    return syscall_names[sys];
}
