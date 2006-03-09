#ifndef JOS_INC_TYPES_H
#define JOS_INC_TYPES_H

#ifdef JOS_KERNEL
#include <machine/types.h>
#endif

#ifdef JOS_USER
#include <sys/types.h>
#include <stdint.h>

// The macros below are also defined in kern/arch/amd64/types.h.
// Would be nice to unify them..

#define MIN(_a, _b)                                             \
({                                                              \
        __typeof__(_a) __a = (_a);                              \
        __typeof__(_b) __b = (_b);                              \
        __a <= __b ? __a : __b;                                 \
})
#define MAX(_a, _b)                                             \
({                                                              \
        __typeof__(_a) __a = (_a);                              \
        __typeof__(_b) __b = (_b);                              \
        __a >= __b ? __a : __b;                                 \
})

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)                         \
({                                              \
        uintptr_t __ra = (uintptr_t) (a);       \
        (__typeof__(a)) (__ra - __ra % (n));    \
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)                                                   \
({                                                                      \
        uintptr_t __n = (uintptr_t) (n);                                \
        (__typeof__(a)) (ROUNDDOWN((uintptr_t) (a) + __n - 1, __n));    \
})

#endif

#if !defined(JOS_KERNEL) && !defined(JOS_USER)
#include <machine/types.h>
#endif

#endif
