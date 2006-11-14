#ifndef JOS_INC_SAFEINT_H
#define JOS_INC_SAFEINT_H

extern int safeint_overflow;

#define SAFE_ADD(a, b)				\
    {						\
	__typeof__(a) __a = (a);		\
	__typeof__(b) __b = (b);		\
	__typeof__(a) __r = __a + __b;		\
	if (__r < __a || __r < __b)		\
	    safeint_overflow = 1;		\
	__r;					\
    }

#define SAFE_MUL(a, b)				\
    {						\
	__typeof__(a) __a = (a);		\
	__typeof__(b) __b = (b);		\
	__typeof__(a) __r = __a * __b;		\
	if (__r / __a != __b)			\
	    safeint_overflow = 1;		\
	__r;					\
    }

#endif
