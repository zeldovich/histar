#ifndef JOS_MACHINE_TYPES_H
#define JOS_MACHINE_TYPES_H

/*
 * Sadly, cygwin has its own <machine/types.h>, so we need to pass it through.
 */
#if __CYGWIN__
#include_next <machine/types.h>
#endif

#include <inttypes.h>
#include <sys/types.h>

typedef uintptr_t physaddr_t;
typedef uintptr_t ppn_t;

typedef int bool_t;

#include <stddef.h>

#endif
