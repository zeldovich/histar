#ifndef JOS_MACHINE_ATOMIC32_H
#define JOS_MACHINE_ATOMIC32_H

typedef struct { volatile uint32_t counter; } jos_atomic_t;

#define JOS_ATOMIC_INIT(i)		{ (i) }
#define jos_atomic_read(v)		((v)->counter)

static __inline__ void
jos_atomic_set(jos_atomic_t *v, uint32_t i)
{
    /* XXX not atomic */
    v->counter = i;
}

static __inline__ void
jos_atomic_inc(jos_atomic_t *v)
{
    /* XXX not atomic */
    v->counter++;
}

static __inline__ void
jos_atomic_dec(jos_atomic_t *v)
{
    /* XXX not atomic */
    v->counter--;
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test(jos_atomic_t *v)
{
    /* XXX not atomic */
    v->counter--;
    return (v->counter == 0);
}

/*
 * Atomically compare the value in "v" with "old", and set "v" to "newv"
 * if equal.
 *
 * Return value is the previous value of "v".  So if return value is same
 * as "old", the swap occurred, otherwise it did not.
 */
static __inline__ int
jos_atomic_compare_exchange(jos_atomic_t *v, uint32_t old, uint32_t newv)
{
    /* XXX not atomic */
    uint32_t cur = v->counter;
    if (cur == old)
	v->counter = newv;
    return cur;
}

#endif
