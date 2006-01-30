#ifndef JOS_INC_MALLOC_ARCH_H
#define JOS_INC_MALLOC_ARCH_H

#include <machine/types.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_STDLIB_H
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H

#define HAVE_MMAP	0
#define ABORT		assert(0)
#define MALLOC_FAILURE_ACTION	do { } while (0)

#endif
