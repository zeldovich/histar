#ifndef JOS_KERN_INTR_H
#define JOS_KERN_INTR_H

#include <machine/types.h>
#include <machine/io.h>
#include <inc/queue.h>

struct interrupt_handler {
    void   (*ih_func) (void*);
    void*    ih_arg;
    tbdp_t   ih_tbdp;
    uint32_t ih_irq;

    LIST_ENTRY(interrupt_handler) ih_link;
};

void	irq_handler(uint32_t trapno);
void	irq_register(struct interrupt_handler *ih);
void	irq_init(void);

#endif
