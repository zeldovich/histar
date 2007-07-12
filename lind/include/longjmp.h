#ifndef LINUX_ARCH_INCLUDE_LONGJMP_H
#define LINUX_ARCH_INCLUDE_LONGJMP_H

#include <sysdep/setjmp.h>
#include <kern/signal.h>

extern int lind_setjmp(jmp_buf);
extern void lind_longjmp(jmp_buf, int);

#define LIND_LONGJMP(buf, val) do { \
	lind_longjmp(*buf, val);	\
} while(0)

#define LIND_SETJMP(buf) ({ \
	int n;	   \
	volatile int enable;	\
	enable = get_signals(); \
	n = lind_setjmp(*buf); \
	if(n != 0) \
		set_signals(enable); \
	n; })

#endif
