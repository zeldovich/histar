#ifndef JOS_INC_BITOPS_H
#define JOS_INC_BITOPS_H

/* Arch. independent (slow) bit operations */

static __inline void
bit_clear(void* bv, uint64_t bit)
{
    ((char *)bv)[bit / 8] &= ~(1 << bit % 8);
}

static __inline void
bit_set(void* bv, uint64_t bit)
{
    ((char *)bv)[bit / 8] |= (1 << bit % 8);
}

static __inline char
bit_get(void* bv, uint64_t bit)
{
    return !!(((char *)bv)[bit / 8] & (1 << bit % 8));
}

#endif
