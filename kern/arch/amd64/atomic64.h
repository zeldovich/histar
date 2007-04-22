#ifndef JOS_MACHINE_ATOMIC64_H
#define JOS_MACHINE_ATOMIC64_H

/*
 * If we support SMP at some point, we should enable this.
 */
#if 0
#define ATOMIC_LOCK "lock ; "
#else
#define ATOMIC_LOCK ""
#endif

typedef struct { volatile uint64_t counter; } jos_atomic64_t;

static __inline__ void
jos_atomic_inc64(jos_atomic64_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "incq %0"
	: "+m" (v->counter)
	:
	: "cc");
}

static __inline__ void
jos_atomic_dec64(jos_atomic64_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "decq %0"
	: "+m" (v->counter)
	:
	: "cc");
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test64(jos_atomic64_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
	ATOMIC_LOCK "decq %0; sete %1"
	: "+m" (v->counter), "=qm" (c)
	:
	: "cc");
    return c != 0;
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
    uint64_t out;
    __asm__ __volatile__(
	ATOMIC_LOCK "cmpxchgq %2,%1"
	: "=a" (out), "+m" (v->counter)
	: "q" (newv), "0" (old)
	: "cc");
    return out;
}

#undef ATOMIC_LOCK
#endif
