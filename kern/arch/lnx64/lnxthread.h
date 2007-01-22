#ifndef JOS_MACHINE_LNXTHREAD_H
#define JOS_MACHINE_LNXTHREAD_H

#include <machine/types.h>
#include <kern/thread.h>

typedef void (*lnx64_thread_cb_t)(void *arg, struct Thread *t);

void lnx64_schedule_loop(void);
void lnx64_set_thread_cb(kobject_id_t tid, lnx64_thread_cb_t cb, void *arg);

#endif
