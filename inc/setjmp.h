#ifndef JOS_INC_SETJMP_H
#define JOS_INC_SETJMP_H

#define F_jos_setjmp	jos_setjmp
#define F_jos_longjmp	jos_longjmp

#include <machine/setjmp.h>

int  F_jos_setjmp(volatile struct jos_jmp_buf *buf);
void F_jos_longjmp(volatile struct jos_jmp_buf *buf, int val)
	__attribute__((__noreturn__, JOS_LONGJMP_GCCATTR));

#endif
