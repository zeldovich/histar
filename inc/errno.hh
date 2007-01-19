#ifndef JOS_INC_ERRNO_HH
#define JOS_INC_ERRNO_HH

#include <errno.h>
#include <string.h>
#include <inc/error.hh>

#define errno_check(expr)						\
    do {								\
	int64_t __r = (expr);						\
	if (__r < 0)							\
	    throw basic_exception("%s:%u: %s - %s",			\
				  __FILE__, __LINE__, #expr,		\
				  strerror(errno));			\
    } while (0)

#endif
