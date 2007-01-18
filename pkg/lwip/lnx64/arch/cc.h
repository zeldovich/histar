#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
#define U16_F	"u"
#define X16_F	"x"

#define S32_F	"d"
#define U32_F	"u"
#define X32_F	"x"

#define lwip_panic(...)				\
    do {					\
	printf("panic: ");			\
	printf(__VA_ARGS__);			\
	abort();				\
    } while(0)

#define _lwip_panic(__x)			\
    do {					\
	printf("panic: %s\n", __x);		\
	abort();				\
    } while(0)

#define LWIP_PLATFORM_DIAG(x)	printf x
#define LWIP_PLATFORM_ASSERT(x)	_lwip_panic(x)

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif
