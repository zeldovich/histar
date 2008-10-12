/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_PICIRQ_H
#define JOS_KERN_PICIRQ_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>

void	 pic_init(void);
void	 pic_eoi(void);
uint32_t pic_intrenable(uint32_t irq);

#endif // !JOS_KERN_PICIRQ_H
