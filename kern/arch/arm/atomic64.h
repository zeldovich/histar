#ifndef JOS_MACHINE_ATOMIC64_H
#define JOS_MACHINE_ATOMIC64_H

typedef struct { volatile uint64_t counter; } jos_atomic64_t;

/*
 * Lazy atomics: the kernel is single-threaded and cannot be preempted,
 * aside from fatal kernel faults. So if atomics are needed in userspace,
 * do it via a syscall.
 */

// Declare here, rather than include <inc/syscall.h> to avoid circular dep.
#ifndef JOS_KERNEL
extern void sys_jos_atomic_set64(jos_atomic64_t *, uint64_t);
extern void sys_jos_atomic_inc64(jos_atomic64_t *);
extern void sys_jos_atomic_dec64(jos_atomic64_t *);
extern void sys_jos_atomic_dec_and_test64(jos_atomic64_t *, int *);
extern void sys_jos_atomic_compare_exchange64(jos_atomic64_t *, uint64_t, uint64_t, uint64_t *);
#endif

static __inline__ void
jos_atomic_set64(jos_atomic64_t *v, uint64_t i)
{
#ifdef JOS_KERNEL
	v->counter = i;
#else
	sys_jos_atomic_set64(v, i);
#endif
}

static __inline__ void
jos_atomic_inc64(jos_atomic64_t *v)
{
#ifdef JOS_KERNEL
	v->counter++;
#else
	sys_jos_atomic_inc64(v);
#endif
}

static __inline__ void
jos_atomic_dec64(jos_atomic64_t *v)
{
#ifdef JOS_KERNEL
	v->counter--;
#else
	sys_jos_atomic_dec64(v);
#endif
}

/* Returns true if result is zero. */
static __inline__ int
jos_atomic_dec_and_test64(jos_atomic64_t *v)
{
#ifdef JOS_KERNEL
	v->counter--;
	return (v->counter == 0);
#else
	int ret;
	sys_jos_atomic_dec_and_test64(v, &ret);
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
static __inline__ uint64_t
jos_atomic_compare_exchange64(jos_atomic64_t *v, uint64_t old, uint64_t newv)
{
#ifdef JOS_KERNEL
	uint64_t cur = jos_atomic_read(v);
	if (cur == old)
		jos_atomic_set64(v, newv);
	return (cur);
#else
	uint64_t ret;
	sys_jos_atomic_compare_exchange64(v, old, newv, &ret);
	return (ret);
#endif
}

#endif
