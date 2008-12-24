#ifndef JOS_MACHINE_ERROR_H
#define JOS_MACHINE_ERROR_H

#include <errno.h>
#include <stdio.h>

extern char *strerror(int errnum);

#define errno_check(expr)				\
    do {						\
        int __r = (expr);				\
	if (__r < 0) {					\
	    fprintf(stderr, "%s:%u: %s - %s\n",		\
		    __FILE__, __LINE__, #expr,		\
		    strerror(errno));			\
	    exit(EXIT_FAILURE);				\
	}						\
    } while (0)

#define eprint(__frmt, __args...)			\
    do {						\
       fprintf(stderr, "%s:%u: " __frmt,		\
	       __FILE__, __LINE__, ##__args);		\
       exit(EXIT_FAILURE);				\
    } while (0)

#endif
