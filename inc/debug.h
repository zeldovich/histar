#ifndef JOS_INC_DEBUG_H_
#define JOS_INC_DEBUG_H_

#include <inc/lib.h>
#include <inttypes.h>
#include <stdio.h>

#define debug_print(__exp, __frmt, __args...) \
    do {                    \
    if (__exp)                \
        printf("(debug) %s: " __frmt "\n", __FUNCTION__, ##__args);   \
    } while (0)

#define debug_cprint(__exp, __frmt, __args...) \
    do {                    \
    if (__exp)                \
        cprintf("(debug) %s: " __frmt "\n", __FUNCTION__, ##__args);   \
    } while (0)

#define jos_trace(__fmt, __args...)					\
    do {								\
	if (start_env->jos_trace_on) {					\
	    char save = start_env->jos_trace_on;			\
	    start_env->jos_trace_on = 0;				\
	    fprintf(stderr, "(%s:%"PRIu64") %s: " __fmt "\n",		\
		    jos_progname, start_env->shared_container,		\
		    __func__, ##__args);				\
	    start_env->jos_trace_on = save;				\
	}								\
    } while (0)

#endif 
