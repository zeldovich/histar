#ifndef JOS_MACHINE_PARAM_H
#define JOS_MACHINE_PARAM_H

#define JOS_ARCH_BITS	32

#ifdef JOS_KERNEL
enum { kobj_hash_size = 8192 };
enum { kobj_neg_size = 16 };
enum { part_enable = 1 };
#endif

#endif
