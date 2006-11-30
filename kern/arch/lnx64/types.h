#ifndef JOS_MACHINE_TYPES_H
#define JOS_MACHINE_TYPES_H

#ifdef JOS_KERNEL
#include <sys/types.h>
#include <inttypes.h>
#endif

#ifndef NULL
#define NULL (0)
#endif

#ifndef inline
#define inline __inline__
#endif

// Represents true-or-false values
typedef int bool_t;

// Explicitly-sized versions of integer types
typedef u_int8_t  uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;

// Fake 128-bit values, used only for the scheduler anyway...
typedef   int64_t  int128_t;
typedef u_int64_t uint128_t;

// Page numbers are 64 bits long.
typedef uint64_t ppn_t;
typedef uint64_t physaddr_t;

// Efficient min and max operations
#define MIN(_a, _b)						\
({								\
	__typeof__(_a) __a = (_a);				\
	__typeof__(_b) __b = (_b);				\
	__a <= __b ? __a : __b;					\
})
#define MAX(_a, _b)						\
({								\
	__typeof__(_a) __a = (_a);				\
	__typeof__(_b) __b = (_b);				\
	__a >= __b ? __a : __b;					\
})

// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)				\
({						\
	uintptr_t __ra = (uintptr_t) (a);	\
	(__typeof__(a)) (__ra - __ra % (n));	\
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)							\
({									\
	uintptr_t __n = (uintptr_t) (n);				\
	(__typeof__(a)) (ROUNDDOWN((uintptr_t) (a) + __n - 1, __n));	\
})

#include <stddef.h>	// gcc header file

#endif /* !JOS_INC_TYPES_H */
