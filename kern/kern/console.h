#ifndef JOS_KERN_CONSOLE_H
#define JOS_KERN_CONSOLE_H

#include <kern/thread.h>
#include <inc/queue.h>

extern struct Thread_list console_waiting;

struct cons_device {
    void *cd_arg;
    void (*cd_pollin) (void *);
    void (*cd_output) (void *, int);
    LIST_ENTRY(cons_device) cd_link;
};

void cons_putc(int c);
int  cons_getc(void);
int  cons_probe(void);
void cons_intr(int (*proc)(void*), void *arg);
void cons_register(struct cons_device *cd);

#endif
