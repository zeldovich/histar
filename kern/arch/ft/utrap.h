#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>
#include <machine/mmu.h>

/*
 * The layout of this structure has to match the DWARF2 hints
 * in lib/amd64/trapstub.S
 */

struct UTrapframe {
    struct Trapframe utf_tf;

    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#endif
