#ifndef JOS_INC_STDDEF_H
#define JOS_INC_STDDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __defined_size_t
#define __defined_size_t
typedef unsigned long size_t;
#endif

#ifndef __defined_ptrdiff_t
#define __defined_ptrdiff_t
typedef long ptrdiff_t;
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef offsetof
#define offsetof(type, member)  ((size_t) (&((type*)0)->member))
#endif

#ifdef __cplusplus
}
#endif

#endif
