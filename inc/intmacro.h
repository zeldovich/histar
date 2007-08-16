#ifndef JOS_INC_INTMACRO_H
#define JOS_INC_INTMACRO_H

/*
 * 64-bit constants
 */
#if __LONG_MAX__==9223372036854775807L
# define UINT64(x) x##UL
# define CAST64(x) ((unsigned long) (x))
#elif __LONG_LONG_MAX__==9223372036854775807LL
# define UINT64(x) x##ULL
# define CAST64(x) ((unsigned long long) (x))
#else
# error Missing 64-bit type
#endif

/*
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 */
#define ROUNDDOWN(a, n)                         \
({                                              \
        uintptr_t __ra = (uintptr_t) (a);       \
        (__typeof__(a)) (__ra - __ra % (n));    \
})

/*
 * Round up to the nearest multiple of n
 */
#define ROUNDUP(a, n)                                                   \
({                                                                      \
        uintptr_t __n = (uintptr_t) (n);                                \
        (__typeof__(a)) (ROUNDDOWN((uintptr_t) (a) + __n - 1, __n));    \
})

/*
 * Efficient min and max operations
 */
#define JMIN(_a, _b)						\
({								\
	__typeof__(_a) __a = (_a);				\
	__typeof__(_b) __b = (_b);				\
	__a <= __b ? __a : __b;					\
})
#define JMAX(_a, _b)						\
({								\
	__typeof__(_a) __a = (_a);				\
	__typeof__(_b) __b = (_b);				\
	__a >= __b ? __a : __b;					\
})

/*
 * Check for power-of-2, works for static_assert()
 */
#define IS_POWER_OF_2(n) (!(((n) - 1) & (n)))

#endif
