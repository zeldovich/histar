#ifndef JOS_INC_SETJMP_H
#define JOS_INC_SETJMP_H

#include <machine/setjmp.h>

int  setjmp(struct jmp_buf *buf);
void longjmp(struct jmp_buf *buf, uint64_t val) __attribute__((__noreturn__));

typedef struct jmp_buf sigjmp_buf[1];
int sigsetjmp(struct jmp_buf env[1], int savesigs);

#endif
