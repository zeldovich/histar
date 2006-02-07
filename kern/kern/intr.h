#ifndef JOS_KERN_INTR_H
#define JOS_KERN_INTR_H

#include <inc/queue.h>

struct interrupt_handler {
    void (*ih_func) (void);
    LIST_ENTRY(interrupt_handler) ih_link;
};

void	irq_handler(int irqno);
void	irq_register(int irq, struct interrupt_handler *ih);

#endif
