#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <inc/types.h>
#include <inc/string.h>
#include <inc/assert.h>

typedef uint32_t u32_t;
typedef int32_t s32_t;

typedef uint16_t u16_t;
typedef int16_t s16_t;

typedef uint8_t u8_t;
typedef int8_t s8_t;

typedef uintptr_t mem_ptr_t;

#define PACK_STRUCT_FIELD(x)	x
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define S16_F	"d"

#define LWIP_PLATFORM_DIAG(x)	cprintf x
#define LWIP_PLATFORM_ASSERT(x)	panic(x)

// Our runtime is so incompetent..
#define LWIP_PROVIDE_ERRNO

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif
