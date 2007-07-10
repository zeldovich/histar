#ifndef JOS_MACHINE_ATOMIC32_H
#define JOS_MACHINE_ATOMIC32_H

typedef struct { volatile uint32_t counter; } jos_atomic_t;

#define JOS_ATOMIC_INIT(i)		{ (i) }
#define jos_atomic_read(v)		((v)->counter)

#endif
