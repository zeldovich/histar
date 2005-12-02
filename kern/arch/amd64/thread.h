#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>
#include <kern/label.h>
#include <inc/queue.h>
#include <inc/segment.h>

typedef enum {
    thread_runnable,
    thread_suspended,
    thread_halted,
    thread_not_started
} thread_status;

struct Thread {
    struct Trapframe th_tf __attribute__ ((aligned (16)));

    struct Label *th_label;
    struct Pagemap *th_pgmap;
    struct segment_map th_segmap;

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
void thread_decref(struct Thread *t);
void thread_free(struct Thread *t);

// Assumes ownership of label
void thread_jump(struct Thread *t, struct Label *label, struct segment_map *segmap,
		 void *entry, void *stack, uint64_t arg);
void thread_syscall_restart(struct Thread *t);

void thread_set_runnable(struct Thread *t);
void thread_suspend(struct Thread *t);
void thread_halt(struct Thread *t);

void thread_switch(struct Thread *t);
void thread_run(struct Thread *t) __attribute__((__noreturn__));

int  thread_pagefault(void *va);

#endif
