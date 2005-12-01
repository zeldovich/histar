#ifndef JOS_INC_SETJMP_H
#define JOS_INC_SETJMP_H

#include <machine/setjmp.h>

int  setjmp(struct jmp_buf *buf);
void longjmp(struct jmp_buf *buf, uint64_t val) __attribute__((__noreturn__));

#endif
