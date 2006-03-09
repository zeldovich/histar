#ifndef JOS_INC_STDDEF_H
#define JOS_INC_STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

typedef __uint64_t size_t;

#ifndef NULL
#define NULL 0
#endif

#ifdef __cplusplus
}
#endif

#endif
