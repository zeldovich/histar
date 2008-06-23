#ifndef JOS_MACHINE_MP_H
#define JOS_MACHINE_MP_H

#include <machine/types.h>

void mp_init(void);

struct cpu {
    uint8_t apicid;
    volatile char booted;
};

extern struct cpu cpus[];
extern uint32_t ncpu;
extern struct cpu *bcpu;

#endif
