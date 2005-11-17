#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>

struct Thread {
    struct Trapframe tf __attribute__ ((aligned (16)));

    uint64_t *pgmap;
    uint32_t cr3;
};

void thread_create_first(struct Thread *t, uint8_t *binary, size_t size);
void thread_run(struct Thread *t);

// Convenience macro for embedded ELF binaries
#define THREAD_CREATE_EMBED(t, name)				\
    do {							\
	extern uint8_t _binary_obj_##name##_start[],		\
		       _binary_obj_##name##_size[];		\
	thread_create_first(t,					\
			    _binary_obj_##name##_start,		\
			    (int) _binary_obj_##name##_size);	\
    } while (0)

#endif
