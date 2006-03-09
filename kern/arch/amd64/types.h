#ifndef JOS_INC_TYPES_H
#define JOS_INC_TYPES_H

#ifndef NULL
#define NULL (0)
#endif

#ifndef inline
#define inline __inline__
#endif

// Represents true-or-false values
typedef int bool_t;

// Explicitly-sized versions of integer types
typedef __signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
#ifndef CODE32
typedef long int64_t;
typedef unsigned long uint64_t;
#else /* CODE32 */
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif /* CODE32 */
typedef uint64_t __uint64_t;

// Pointers and addresses are 64 bits long.
// We use pointer types to represent virtual addresses,
// uintptr_t to represent the numerical values of virtual addresses,
// and physaddr_t to represent physical addresses.
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef uint64_t physaddr_t;

// Page numbers are 32 bits long.
typedef uint64_t ppn_t;

// size_t is what sizeof returns
#ifndef __defined_size_t
#define __defined_size_t
typedef unsigned long size_t;
#endif

// ssize_t is a signed version of ssize_t, used in case there might be an
// error return.
typedef long ssize_t;

// ptrdiff_t is the result of subtracting two pointers
#ifndef __defined_ptrdiff_t
#define __defined_ptrdiff_t
typedef long ptrdiff_t;
#endif

// off_t is used for file offsets and lengths.
typedef int32_t off_t;

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

// Return the offset of 'member' relative to the beginning of a struct type
#define offsetof(type, member)  ((size_t) (&((type*)0)->member))

typedef uint32_t time_t;

#endif /* !JOS_INC_TYPES_H */
