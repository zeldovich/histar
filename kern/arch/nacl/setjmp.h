#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#ifdef JOS_KERNEL
#include <setjmp.h>

struct jos_jmp_buf {
    jmp_buf native_jb;
};

#undef F_jos_setjmp
#define jos_setjmp(buf) __sigsetjmp(((struct jos_jmp_buf *) buf)->native_jb, 0)
#define JOS_LONGJMP_GCCATTR
#endif

#ifdef JOS_USER
#include <inc/types.h>

#define JOS_LONGJMP_GCCATTR	regparm(2)

struct jos_jmp_buf {
    uint32_t jb_eip;
    uint32_t jb_esp;
    uint32_t jb_ebp;
    uint32_t jb_ebx;
    uint32_t jb_esi;
    uint32_t jb_edi;
};
#endif

#endif
