#ifndef JOS_INC_ARRAY_H
#define JOS_INC_ARRAY_H

#include <machine/compiler.h>
#ifdef JOS_USER
#include <inc/assert.h>
#endif

#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]) + __is_array(arr))
#define array_end(arr) ((arr) + array_size(arr))

#endif
