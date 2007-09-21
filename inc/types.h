#ifndef JOS_INC_TYPES_H
#define JOS_INC_TYPES_H

#ifdef JOS_KERNEL
#include <machine/types.h>
#endif

#ifdef JOS_USER
#include <stdint.h>
#include <sys/types.h>
#include <sys/param.h>
#endif

#if !defined(JOS_KERNEL) && !defined(JOS_USER)
#include <machine/types.h>
#endif

#endif
