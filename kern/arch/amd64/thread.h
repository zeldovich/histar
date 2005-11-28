#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>
#include <inc/queue.h>
#include <kern/label.h>

typedef enum {
    thread_runnable,
    thread_not_runnable
} thread_status;

struct Thread {
    struct Trapframe th_tf __attribute__ ((aligned (16)));

    struct Label *th_label;
    struct Pagemap *th_pgmap;
    uint64_t th_cr3;

    uint32_t th_ref;
    thread_status th_status;

    LIST_ENTRY(Thread) th_link;
    TAILQ_ENTRY(Thread) th_waiting;
};

LIST_HEAD(Thread_list, Thread);
TAILQ_HEAD(Thread_tqueue, Thread);

extern struct Thread_list thread_list;
extern struct Thread *cur_thread;

int  thread_alloc(struct Thread **tp);
int  thread_load_elf(struct Thread *t, struct Label *label, uint8_t *binary, size_t size);
void thread_set_runnable(struct Thread *t);
void thread_suspend(struct Thread *t);
void thread_decref(struct Thread *t);
void thread_free(struct Thread *t);

int  thread_jump(struct Thread *t, struct Label *label, struct Pagemap *pgmap, void *entry, uint64_t arg);
void thread_syscall_restart(struct Thread *t);

void thread_run(struct Thread *t) __attribute__((__noreturn__));
void thread_halt(struct Thread *t);

// Convenience macro for embedded ELF binaries
#define THREAD_CREATE_EMBED(container, label, name)		\
    do {							\
	int r;							\
	struct Thread *t;					\
	extern uint8_t _binary_obj_##name##_start[],		\
		       _binary_obj_##name##_size[];		\
								\
	r = thread_alloc(&t);					\
	if (r < 0) panic("cannot alloc thread");		\
								\
	r = thread_load_elf(t, label,				\
			    _binary_obj_##name##_start,		\
			    (int) _binary_obj_##name##_size);	\
	if (r < 0) panic("cannot load elf");			\
								\
	r = container_put(container, cobj_thread, t);		\
	if (r < 0) panic("cannot add to container");		\
								\
	thread_set_runnable(t);					\
    } while (0)

#endif
