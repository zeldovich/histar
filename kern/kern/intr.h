#ifndef JOS_KERN_INTR_H
#define JOS_KERN_INTR_H

#include <machine/types.h>
#include <inc/queue.h>

struct interrupt_handler {
    void (*ih_func) (void *);
    void *ih_arg;
    LIST_ENTRY(interrupt_handler) ih_link;
};

void	irq_handler(uint32_t irqno);
void	irq_register(uint32_t irq, struct interrupt_handler *ih);
int64_t	irq_wait_thread(uint32_t irq, int64_t lastcount);

#endif
