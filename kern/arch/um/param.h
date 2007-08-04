#ifndef JOS_MACHINE_PARAM_H
#define JOS_MACHINE_PARAM_H

#define JOS_ARCH_BITS	32
#define JOS_ARCH_ENDIAN	JOS_LITTLE_ENDIAN

#ifdef JOS_KERNEL
enum { kobj_hash_size = 8192 };
enum { kobj_neg_size = 2 };
enum { kobj_neg_hash = 64 };
enum { part_enable = 0 };

#define MAX_IRQS	16
#endif

#endif
