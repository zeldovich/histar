#ifndef JOS_KERN_CONSOLE_H
#define JOS_KERN_CONSOLE_H

#include <kern/thread.h>
#include <inc/queue.h>

extern struct Thread_list console_waiting;

typedef SAFE_TYPE(int) cons_source;
#define cons_source_user	SAFE_WRAP(cons_source, 1)
#define cons_source_kernel	SAFE_WRAP(cons_source, 2)

struct cons_device {
    void *cd_arg;
    int  (*cd_pollin) (void *);
    void (*cd_output) (void *, int, cons_source);
    LIST_ENTRY(cons_device) cd_link;
};

void cons_putc(int c, cons_source src);
int  cons_getc(void);
int  cons_probe(void);
void cons_intr(int (*proc)(void*), void *arg);
void cons_register(struct cons_device *cd);

#endif
