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

    struct cobj_ref th_asref;
    const struct Address_space *th_as;

    struct Trapframe th_tf __attribute__ ((aligned (16)));
    struct Label th_clearance;

    // The thread's associated segment & container
    kobject_id_t th_sg;
    kobject_id_t th_ct;

    thread_status th_status;
    bool_t th_pinned;

    uint64_t th_wakeup_msec;
    uint64_t th_wakeup_addr;

    LIST_ENTRY(Thread) th_link;
};

LIST_HEAD(Thread_list, Thread);

extern struct Thread_list thread_list_runnable;
extern struct Thread_list thread_list_limbo;
extern const struct Thread *cur_thread;

int  thread_alloc(const struct Label *l,
		  const struct Label *clearance,
		  struct Thread **tp);
void thread_swapin(struct Thread *t);
void thread_swapout(struct Thread *t);
int  thread_gc(struct Thread *t)
    __attribute__ ((warn_unused_result));
void thread_zero_refs(const struct Thread *t);

int  thread_jump(const struct Thread *t,
		 const struct Label *label,
		 const struct Label *clearance,
		 struct cobj_ref as, void *entry, void *stack,
		 uint64_t arg0, uint64_t arg1)
    __attribute__ ((warn_unused_result));
int  thread_change_label(const struct Thread *t, const struct Label *l)
    __attribute__ ((warn_unused_result));
void thread_change_as(const struct Thread *t, struct cobj_ref as);
void thread_syscall_restart(const struct Thread *t);

void thread_set_runnable(const struct Thread *t);
void thread_suspend(const struct Thread *t, struct Thread_list *waitq);
void thread_halt(const struct Thread *t);

void thread_switch(const struct Thread *t);
void thread_run(const struct Thread *t) __attribute__((__noreturn__));

int  thread_pagefault(const struct Thread *t, void *va)
    __attribute__ ((warn_unused_result));

#endif
