#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>
#include <machine/as.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>
#include <kern/container.h>
#include <inc/queue.h>

typedef enum {
    thread_runnable,
    thread_suspended,
    thread_halted,
    thread_not_started
} thread_status;

struct Thread {
    struct kobject_hdr th_ko;

    struct Trapframe th_tf __attribute__ ((aligned (16)));

    struct cobj_ref th_asref;
    const struct Address_space *th_as;

    thread_status th_status;
    bool_t th_pinned;
    uint64_t th_wakeup_ticks;

    LIST_ENTRY(Thread) th_link;
};

LIST_HEAD(Thread_list, Thread);

extern struct Thread_list thread_list_runnable;
extern struct Thread_list thread_list_limbo;
extern struct Thread *cur_thread;

int  thread_alloc(struct Label *l, struct Thread **tp);
void thread_swapin(struct Thread *t);
void thread_swapout(struct Thread *t);
int  thread_gc(struct Thread *t);

int  thread_jump(struct Thread *t, const struct Label *label,
		 struct cobj_ref as, void *entry, void *stack,
		 uint64_t arg0, uint64_t arg1, uint64_t arg2);
void thread_syscall_restart(struct Thread *t);

void thread_set_runnable(struct Thread *t);
void thread_suspend(struct Thread *t, struct Thread_list *waitq);
void thread_halt(struct Thread *t);

void thread_switch(struct Thread *t);
void thread_run(struct Thread *t) __attribute__((__noreturn__));

int  thread_pagefault(struct Thread *t, void *va);

#endif
