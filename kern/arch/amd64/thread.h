#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>
#include <inc/queue.h>

typedef enum {
    thread_runnable,
    thread_not_runnable
} thread_status;

struct Thread {
    struct Trapframe th_tf __attribute__ ((aligned (16)));

    struct Pagemap *th_pgmap;
    uint64_t th_cr3;

    thread_status th_status;

    LIST_ENTRY(Thread) th_link;
};

LIST_HEAD(Thread_list, Thread);
extern struct Thread_list thread_list;
extern struct Thread *cur_thread;

int  thread_alloc(struct Thread **tp);
int  thread_load_elf(struct Thread *t, uint8_t *binary, size_t size);
void thread_set_runnable(struct Thread *t);
void thread_free(struct Thread *t);

void thread_run(struct Thread *t);
void thread_kill(struct Thread *t);

// Convenience macro for embedded ELF binaries
#define THREAD_CREATE_EMBED(name)				\
    do {							\
	int r;							\
	struct Thread *t;					\
	extern uint8_t _binary_obj_##name##_start[],		\
		       _binary_obj_##name##_size[];		\
								\
	r = thread_alloc(&t);					\
	if (r < 0) panic("cannot alloc thread");		\
	r = thread_load_elf(t,					\
			    _binary_obj_##name##_start,		\
			    (int) _binary_obj_##name##_size);	\
	if (r < 0) panic("cannot load elf");			\
	thread_set_runnable(t);					\
    } while (0)

#endif
