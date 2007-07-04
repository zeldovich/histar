#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#include <inc/types.h>

#define JOS_LONGJMP_GCCATTR	

struct jos_jmp_buf {
    /* For x86 style stack management.
     * We don't need all the globals, only %g2-%g4.
     * The 8-byte alignment is so we can use ldd/std.
     */
    uint32_t jb_sp;
    uint32_t jb_pc;

    uint32_t jb_locals[8];
    uint32_t jb_ins[8];
    uint32_t jb_globals[8];
} __attribute__((aligned (8)));

#endif
