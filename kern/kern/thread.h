#ifndef JOS_KERN_THREAD_H
#define JOS_KERN_THREAD_H

#include <machine/mmu.h>
#include <kern/as.h>
#include <kern/label.h>
#include <kern/kobjhdr.h>
#include <kern/container.h>
#include <inc/queue.h>
#include <inc/thread.h>

typedef SAFE_TYPE(uint8_t) thread_status;
#define thread_not_started	SAFE_WRAP(thread_status, 1)
#define thread_runnable		SAFE_WRAP(thread_status, 2)
#define thread_suspended	SAFE_WRAP(thread_status, 3)
#define thread_halted		SAFE_WRAP(thread_status, 4)

struct Thread {
    struct kobject_hdr th_ko;

    struct Trapframe th_tf __attribute__ ((aligned (16)));
    struct Trapframe_aux th_tfa;

    struct cobj_ref th_asref;
    const struct Address_space *th_as;

    // The thread-local segment
    kobject_id_t th_sg;

    thread_status th_status;
    uint8_t th_pinned : 1;
    uint8_t th_fp_enabled : 1;
    uint8_t th_fp_space : 1;
    uint8_t th_sched_joined : 1;
    uint8_t th_sync_waiting : 1;
    uint8_t th_cache_flush : 1;
    uint32_t th_sched_tickets;
    uint64_t th_multi_slots;

    kobject_id_t th_sched_parents[2];
    union {
	uint128_t th_sched_pass;
	int128_t th_sched_remain;
    };

    LIST_ENTRY(Thread) th_link;
};

LIST_HEAD(Thread_list, Thread);

struct sync_wait_slot {
    uint64_t sw_seg_id;
    uint64_t sw_offset;
    const struct Thread *sw_t;
    LIST_ENTRY(sync_wait_slot) sw_addr_link;
    LIST_ENTRY(sync_wait_slot) sw_thread_link;
};

LIST_HEAD(sync_wait_list, sync_wait_slot);

struct Thread_ephemeral {
    uint64_t te_wakeup_nsec;
    struct sync_wait_list te_wait_slots;
    struct sync_wait_slot te_sync;
};

extern struct Thread_list thread_list_runnable;
extern struct Thread_list thread_list_limbo;
extern const struct Thread *cur_thread, *trap_thread;

int  thread_alloc(const struct Label *contaminate,
		  const struct Label *clearance,
		  struct Thread **tp)
    __attribute__ ((warn_unused_result));
void thread_swapin(struct Thread *t);
void thread_swapout(struct Thread *t);
int  thread_gc(struct Thread *t)
    __attribute__ ((warn_unused_result));
void thread_on_decref(const struct Thread *t);

int  thread_jump(const struct Thread *t,
		 const struct Label *contaminate,
		 const struct Label *clearance,
		 const struct thread_entry *te)
    __attribute__ ((warn_unused_result));
int  thread_change_label(const struct Thread *t, const struct Label *l)
    __attribute__ ((warn_unused_result));
int  thread_load_as(const struct Thread *t)
    __attribute__ ((warn_unused_result));
void thread_change_as(const struct Thread *t, struct cobj_ref as);
int  thread_enable_fp(const struct Thread *t)
    __attribute__ ((warn_unused_result));
void thread_disable_fp(const struct Thread *t);
int  thread_set_waitslots(const struct Thread *t, uint64_t nslots)
    __attribute__ ((warn_unused_result));
void thread_set_sched_parents(const struct Thread *t, uint64_t p1, uint64_t p2);
void thread_check_sched_parents(const struct Thread *t);

void thread_set_runnable(const struct Thread *t);
void thread_suspend(const struct Thread *t, struct Thread_list *waitq);
void thread_halt(const struct Thread *t);

void thread_switch(const struct Thread *t);
void thread_run(const struct Thread *t) __attribute__((__noreturn__));

int  thread_pagefault(const struct Thread *t, void *va, uint32_t reqflags)
    __attribute__ ((warn_unused_result));
int  thread_utrap(const struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
    __attribute__ ((warn_unused_result));

#endif
