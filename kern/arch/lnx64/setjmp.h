#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#include <inc/types.h>

struct jos_jmp_buf {
#if __WORDSIZE == 64
    uint64_t jb_rip;
    uint64_t jb_rsp;
    uint64_t jb_rbp;
    uint64_t jb_rbx;
    uint64_t jb_r12;
    uint64_t jb_r13;
    uint64_t jb_r14;
    uint64_t jb_r15;
#else
    uint32_t jb_rip;
    uint32_t jb_rsp;
    uint32_t jb_ebp;
    uint32_t jb_ebx;
    uint32_t jb_esi;
    uint32_t jb_edi;
#endif
};

#endif
