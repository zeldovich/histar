#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#include <setjmp.h>

struct jos_jmp_buf {
    jmp_buf native_jb;
};

#undef F_jos_setjmp
#define jos_setjmp(buf) __sigsetjmp(((struct jos_jmp_buf *) buf)->native_jb, 0)
#define JOS_LONGJMP_GCCATTR

#endif
