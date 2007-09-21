#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

void lind_intr_timer(void);
void lind_intr_eth(void);
void lind_intr_netd(void);

#define NR_IRQS 1

#endif /* _ASM_IRQ_H */
