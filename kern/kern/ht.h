#ifndef JOS_KERN_HT_H
#define JOS_KERN_HT_H

#include <machine/types.h>

/*
 * Hash table size must be a power of 2.
 */

#define HASH_TABLE(name, type, size)				\
    struct name {						\
	type ht_slot[size];					\
    }

#define HASH_SLOT(table, key)					\
    ({								\
	static_assert(sizeof(key) == sizeof(uint64_t));		\
	uint64_t size = sizeof((table)->ht_slot) /		\
			sizeof((table)->ht_slot[0]);		\
	&((table)->ht_slot[(key) & (size - 1)]);		\
    })

#endif
