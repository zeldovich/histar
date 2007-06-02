#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#include <inc/types.h>

struct jos_jmp_buf {
    /* If we are using register windows, this is sufficient */
    uint32_t jb_sp;
    uint32_t jb_fp;
    uint32_t jb_pc;

    /* Otherwise, we should store 24 registers -- everything except %o? */
};

#endif
