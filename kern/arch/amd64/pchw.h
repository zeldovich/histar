#ifndef JOS_MACHINE_PCHW_H
#define JOS_MACHINE_PCHW_H

#include <machine/x86.h>

static __inline__ __attribute__((always_inline)) void
machine_reboot(void)
{
    outb(0x92, 0x3);
}

#endif
