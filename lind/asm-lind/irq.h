#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#define LIND_TIMER_IRQ	    0
#define LIND_NETD_IRQ       1
#define LIND_ETH_IRQ	    2

#define LAST_IRQ LIND_ETH_IRQ
#define NR_IRQS (LAST_IRQ + 1)

#endif /* _ASM_IRQ_H */
