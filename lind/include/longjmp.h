#ifndef LINUX_ARCH_INCLUDE_LONGJMP_H
#define LINUX_ARCH_INCLUDE_LONGJMP_H

#include <sysdep/setjmp.h>
#include <kern/signal.h>

extern int lind_setjmp(jmp_buf);
extern void lind_longjmp(jmp_buf, int);
extern int lind_irq_enabled;

#define LIND_LONGJMP(buf, val)		\
    do {				\
	lind_longjmp(*buf, val);	\
    } while(0)

#define LIND_SETJMP(buf)		\
    ({					\
	int n;				\
	volatile int enable;		\
	enable = lind_irq_enabled;	\
	n = lind_setjmp(*buf);		\
	if (n)				\
	    lind_irq_enabled = enable;	\
	n;				\
    })

#endif
