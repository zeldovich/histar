#ifndef ATOMIC_H
#define ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

#ifdef CONFIG_SMP
#define ATOMIC_LOCK "lock ; "
#else
#define ATOMIC_LOCK ""
#endif

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { volatile uint32_t counter; } atomic_t;
typedef struct { volatile uint64_t counter; } atomic64_t;

#define ATOMIC_INIT(i)		{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically reads the value of @v.
 */ 
#define atomic_read(v)		((v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 * 
 * Atomically sets the value of @v to @i.
 */ 
#define atomic_set(v,i)		(((v)->counter) = (i))

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 * 
 * Atomically adds @i to @v.
 */
static __inline__ void atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "addl %1,%0"
		:"+m" (v->counter)
		:"ir" (i)
		:"cc");
}

/**
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v.
 */
static __inline__ void atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "subl %1,%0"
		:"+m" (v->counter)
		:"ir" (i)
		:"cc");
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline__ int atomic_sub_and_test(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		ATOMIC_LOCK "subl %2,%0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i)
		:"cc");
	return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1.
 */ 
static __inline__ void atomic_inc(atomic_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "incl %0"
		:"+m" (v->counter)
		:
		:"cc");
}

static __inline__ void atomic_inc64(atomic64_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "incq %0"
		:"+m" (v->counter)
		:
		:"cc");
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.
 */ 
static __inline__ void atomic_dec(atomic_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "decl %0"
		:"+m" (v->counter)
		:
		:"cc");
}

static __inline__ void atomic_dec64(atomic64_t *v)
{
	__asm__ __volatile__(
		ATOMIC_LOCK "decq %0"
		:"+m" (v->counter)
		:
		:"cc");
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */ 
static __inline__ int atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		ATOMIC_LOCK "decl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:
		:"cc");
	return c != 0;
}

static __inline__ int atomic_dec_and_test64(atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		ATOMIC_LOCK "decq %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:
		:"cc");
	return c != 0;
}

/**
 * atomic_inc_and_test - increment and test 
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */ 
static __inline__ int atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		ATOMIC_LOCK "incl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:
		:"cc");
	return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 * 
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */ 
static __inline__ int atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		ATOMIC_LOCK "addl %2,%0; sets %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i)
		:"cc");
	return c;
}

/*
 * Atomically compare the value in "v" with "old", and set "v" to "newv"
 * if equal.
 *
 * Return value is the previous value of "v".  So if return value is same
 * as "old", the swap occurred, otherwise it did not.
 */
static __inline__ int atomic_compare_exchange(atomic_t *v, int old, int newv)
{
	int out;
	__asm__ __volatile__(
		ATOMIC_LOCK "cmpxchgl %2,%1"
		: "=a" (out), "+m" (v->counter)
		: "q" (newv), "0" (old)
		: "cc");
	return out;
}

static __inline__ uint64_t atomic_compare_exchange64(atomic64_t *v, uint64_t old, uint64_t newv)
{
	uint64_t out;
	__asm__ __volatile__(
		ATOMIC_LOCK "cmpxchgq %2,%1"
		: "=a" (out), "+m" (v->counter)
		: "q" (newv), "0" (old)
		: "cc");
	return out;
}

/* These are x86-specific, used by some header files */
#define atomic_clear_mask(mask, addr) \
__asm__ __volatile__(ATOMIC_LOCK "andl %1,%0" \
: "+m" (*(addr)) : "r" (~(mask)) : "cc")

#define atomic_set_mask(mask, addr) \
__asm__ __volatile__(ATOMIC_LOCK "orl %1,%0" \
: "+m" (*(addr)) : "r" (mask) : "cc")

#endif
