#ifndef JOS_INC_SETJMP_H
#define JOS_INC_SETJMP_H

#include <machine/setjmp.h>

int  jos_setjmp(struct jos_jmp_buf *buf);
void jos_longjmp(struct jos_jmp_buf *buf, uint64_t val)
	__attribute__((__noreturn__, regparm(2)));

#endif
