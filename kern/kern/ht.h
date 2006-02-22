#ifndef JOS_KERN_HT_H
#define JOS_KERN_HT_H

#include <machine/types.h>

#define HASH_TABLE(name, type, size)				\
    struct name {						\
	type ht_slot[size];					\
    }

#define HASH_SLOT(table, key)					\
    ({								\
	uint64_t hv = ht_hash((void *) &(key), sizeof(key));	\
	uint64_t size = sizeof((table)->ht_slot) /		\
			sizeof((table)->ht_slot[0]);		\
	&((table)->ht_slot[hv % size]);				\
    })

uint64_t ht_hash(uint8_t *blob, uint32_t len);

#endif
