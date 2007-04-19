#ifndef JOS_MACHINE_TYPES_H
#define JOS_MACHINE_TYPES_H

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
#if __LONG_MAX__==9223372036854775807L
typedef long int64_t;
typedef unsigned long uint64_t;
#elif __LONG_LONG_MAX__==9223372036854775807LL
typedef long long int64_t;
typedef unsigned long long uint64_t;
#else
#error Missing 64-bit type
#endif
typedef __int128_t int128_t;
typedef __uint128_t uint128_t;
typedef uint64_t __uint64_t;

// Pointers and addresses are 64 bits long.
// We use pointer types to represent virtual addresses,
// uintptr_t to represent the numerical values of virtual addresses,
// and physaddr_t to represent physical addresses.
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef uint64_t physaddr_t;

// Page numbers are 64 bits long.
typedef uint64_t ppn_t;

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

#include <stddef.h>	// gcc header file

#define PRIu64 "ld"
#define PRIx64 "lx"

#endif /* !JOS_INC_TYPES_H */
