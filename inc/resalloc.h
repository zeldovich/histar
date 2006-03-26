#ifndef JOS_INC_RESALLOC_H
#define JOS_INC_RESALLOC_H

#include <inc/types.h>

int  resalloc_grow(uint64_t ct);

#define RES_WRAP(ct, expr)				\
    ({							\
	__typeof__(expr) __r;				\
	for (;;) {					\
	    __r = (expr);				\
	    if (__r >= 0 || __r != -E_RESOURCE)		\
		break;					\
	    int __ra = resalloc_grow(ct);		\
	    if (__ra < 0) {				\
		__r = -E_RESOURCE;			\
		break;					\
	    }						\
	}						\
	__r;						\
    })

#endif
