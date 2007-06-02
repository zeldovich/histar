#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>

struct UTrapframe {
    /* XXX missing stuff */

    union {
	uint32_t utf_o6;
	uint32_t utf_stackptr;
    };

    uint32_t utf_pc;

    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#define UTRAP_SRC_HW	1
#define UTRAP_SRC_USER	2

#endif
