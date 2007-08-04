#ifndef JOS_MACHINE_TYPES_H
#define JOS_MACHINE_TYPES_H

/*
 * Sadly, cygwin has its own <machine/types.h>, so we need to pass it through.
 */
#if __CYGWIN__
#include_next <machine/types.h>
#endif

#include <inttypes.h>
#include <sys/types.h>

typedef __uint64_t uint128_t;
typedef __int64_t int128_t;

typedef uintptr_t physaddr_t;
typedef uintptr_t ppn_t;

typedef int bool_t;

// Efficient min and max operations
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

#ifndef NULL
#define NULL (0)
#endif

#include <stddef.h>

#endif
