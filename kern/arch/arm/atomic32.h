#ifndef JOS_MACHINE_ATOMIC32_H
#define JOS_MACHINE_ATOMIC32_H

typedef struct { volatile uint32_t counter; } jos_atomic_t;

/*
 * Lazy atomics: the kernel is single-threaded and cannot be preempted,
 * aside from fatal kernel faults. So if atomics are needed in userspace,
 * do it via a syscall.
 */

// Declare here, rather than include <inc/syscall.h> to avoid circular dep.
#ifndef JOS_KERNEL
extern void sys_jos_atomic_set(jos_atomic_t *, uint32_t);
extern void sys_jos_atomic_inc(jos_atomic_t *);
extern void sys_jos_atomic_dec(jos_atomic_t *);
extern void sys_jos_atomic_dec_and_test(jos_atomic_t *, int *);
extern void sys_jos_atomic_compare_exchange(jos_atomic_t *, uint32_t, uint32_t, uint32_t *);
#endif

#define JOS_ATOMIC_INIT(i)		{ (i) }
#define jos_atomic_read(v)		((v)->counter)

static __inline__ void
jos_atomic_set(jos_atomic_t *v, uint32_t i)
{
#ifdef JOS_KERNEL
	v->counter = i;
#else
	sys_jos_atomic_set(v, i);
#endif
}

static __inline__ void
jos_atomic_inc(jos_atomic_t *v)
{
#ifdef JOS_KERNEL
	v->counter++;
#else
	sys_jos_atomic_inc(v);
#endif
}

static __inline__ void
jos_atomic_dec(jos_atomic_t *v)
{
#ifdef JOS_KERNEL
	v->counter--;
#else
	sys_jos_atomic_dec(v);
#endif
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test(jos_atomic_t *v)
{
#ifdef JOS_KERNEL
	v->counter--;
	return (v->counter == 0);
#else
	int ret;
	sys_jos_atomic_dec_and_test(v, &ret);
	return (ret);
#endif
}

/*
 * Atomically compare the value in "v" with "old", and set "v" to "newv"
 * if equal.
 *
 * Return value is the previous value of "v".  So if return value is same
 * as "old", the swap occurred, otherwise it did not.
 */
static __inline__ uint32_t
jos_atomic_compare_exchange(jos_atomic_t *v, uint32_t old, uint32_t newv)
{
#ifdef JOS_KERNEL
	uint32_t cur = v->counter;
	if (cur == old)
		v->counter = newv;
	return (cur);
#else
	uint32_t ret;
	sys_jos_atomic_compare_exchange(v, old, newv, &ret);
	return (ret);
#endif
}

#endif
