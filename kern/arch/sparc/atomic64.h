#ifndef JOS_MACHINE_ATOMIC64_H
#define JOS_MACHINE_ATOMIC64_H

typedef struct { volatile uint64_t counter; } jos_atomic64_t;

static __inline__ void
jos_atomic_inc64(jos_atomic64_t *v)
{
}

static __inline__ void
jos_atomic_dec64(jos_atomic64_t *v)
{
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test64(jos_atomic64_t *v)
{
    return 0;
}

/*
 * Atomically compare the value in "v" with "old", and set "v" to "newv"
 * if equal.
 *
 * Return value is the previous value of "v".  So if return value is same
 * as "old", the swap occurred, otherwise it did not.
 */
static __inline__ uint64_t
jos_atomic_compare_exchange64(jos_atomic64_t *v, uint64_t old, uint64_t newv)
{
    return 0;
}

#endif
