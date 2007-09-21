#ifndef JOS_INC_CKSUM_H
#define JOS_INC_CKSUM_H

#include <machine/types.h>

uint64_t cksum(uint64_t init, const void *buf, uint64_t count);

#endif
