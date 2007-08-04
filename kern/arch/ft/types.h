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

// 64-bit constants
#if __LONG_MAX__==9223372036854775807L
# define UINT64(x) x##UL
# define CAST64(x) ((unsigned long) (x))
#elif __LONG_LONG_MAX__==9223372036854775807LL
# define UINT64(x) x##ULL
# define CAST64(x) ((unsigned long long) (x))
#else
# error Missing 64-bit type
#endif

#include <stddef.h>	// gcc header file

#endif /* !JOS_INC_TYPES_H */
