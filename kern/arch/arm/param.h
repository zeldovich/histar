#ifndef JOS_MACHINE_PARAM_H
#define JOS_MACHINE_PARAM_H

#include <kern/param.h>

#define JOS_ARCH_BITS	32
#define JOS_ARCH_ENDIAN	JOS_LITTLE_ENDIAN
#define JOS_ARCH_RETADD	1
#define JOS_ARCH_RETADD_ONEOFF	/* only __builtin_return_address(0) is valid */
#define JOS_ARCH_PAGE_BITMAP	1

#ifdef JOS_KERNEL
#define MAX_IRQS	186	/* MSM has 64 vic irqs and 122 gpio irqs */

enum { kobj_hash_size = 8192 };
enum { kobj_neg_size = 2 };
enum { kobj_neg_hash = 64 };
enum { part_enable = 1 };
#endif

#endif
