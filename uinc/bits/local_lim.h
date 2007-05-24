#ifndef UCLIBC_JOS64_LOCAL_LIM_H
#define UCLIBC_JOS64_LOCAL_LIM_H

#include <inc/kobj.h>

#define NAME_MAX	(KOBJ_NAME_LEN-1)
#define PATH_MAX	1024

#define CHILD_MAX	999

#define PIPE_BUF	128

/*
 * Not clear where this should go..  BSD-compat stuff
 */
#define SIZE_T_MAX	UINT_MAX	/* BSD source is not so 64-bit clean */
#define QUAD_MAX	LLONG_MAX

/*
 * Even more questionable
 */
#define _PW_NAME_LEN	15

#endif
