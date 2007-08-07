#ifndef JOS_INC_SAFEINT_H
#define JOS_INC_SAFEINT_H

#define SAFEINT_FUNCTIONS(suffix, T)				\
    static __inline __attribute__((always_inline)) T		\
    safe_add##suffix(int *of, uint64_t a, uint64_t b)		\
    {								\
	T r = a + b;						\
	if (r < a)						\
	    *of = 1;						\
	return r;						\
    }								\
								\
    static __inline __attribute__((always_inline)) T		\
    safe_mul##suffix(int *of, uint64_t a, uint64_t b)		\
    {								\
	T r = a * b;						\
	if (a && r / a != b)					\
	    *of = 1;						\
	return r;						\
    }

SAFEINT_FUNCTIONS(32, uint32_t)
SAFEINT_FUNCTIONS(64, uint64_t)
SAFEINT_FUNCTIONS(ptr, uintptr_t)

#undef SAFEINT_FUNCTIONS

#endif
