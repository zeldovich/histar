#ifndef JOS_MACHINE_ATOMIC32_H
#define JOS_MACHINE_ATOMIC32_H

/*
 * If we support SMP at some point, we should enable this.
 */
#if 0
#define ATOMIC_LOCK "lock ; "
#else
#define ATOMIC_LOCK ""
#endif

typedef struct { volatile uint32_t counter; } jos_atomic_t;

#define JOS_ATOMIC_INIT(i)		{ (i) }
#define jos_atomic_read(v)		((v)->counter)
#define jos_atomic_set(v,i)		(((v)->counter) = (i))

static __inline__ void
jos_atomic_add(int i, jos_atomic_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "addl %1, %0"
	: "+m" (v->counter)
	: "ir" (i)
	: "cc");
}

static __inline__ void
jos_atomic_sub(int i, jos_atomic_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "subl %1, %0"
	: "+m" (v->counter)
	: "ir" (i)
	: "cc");
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_sub_and_test(int i, jos_atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
	ATOMIC_LOCK "subl %2,%0; sete %1"
	: "+m" (v->counter), "=qm" (c)
	: "ir" (i)
	: "cc");
    return c;
}

static __inline__ void
jos_atomic_inc(jos_atomic_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "incl %0"
	: "+m" (v->counter)
	:
	: "cc");
}

static __inline__ void
jos_atomic_dec(jos_atomic_t *v)
{
    __asm__ __volatile__(
	ATOMIC_LOCK "decl %0"
	: "+m" (v->counter)
	:
	: "cc");
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test(jos_atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
	ATOMIC_LOCK "decl %0; sete %1"
	: "+m" (v->counter), "=qm" (c)
	:
	: "cc");
    return c != 0;
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_inc_and_test(jos_atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
	ATOMIC_LOCK "incl %0; sete %1"
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
static __inline__ int
jos_atomic_compare_exchange(jos_atomic_t *v, int old, int newv)
{
    int out;
    __asm__ __volatile__(
	ATOMIC_LOCK "cmpxchgl %2,%1"
	: "=a" (out), "+m" (v->counter)
	: "q" (newv), "0" (old)
	: "cc");
    return out;
}

#undef ATOMIC_LOCK
#endif
