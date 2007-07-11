#ifndef JOS_MACHINE_PARAM_H
#define JOS_MACHINE_PARAM_H

#include <kern/param.h>

#define JOS_ARCH_BITS	64
#define JOS_ARCH_ENDIAN	JOS_LITTLE_ENDIAN

#ifdef JOS_KERNEL
#include <dev/picirq.h>

enum { kobj_hash_size = 8192 };
enum { kobj_neg_size = 16 };
enum { part_enable = 1 };
#endif

#endif
