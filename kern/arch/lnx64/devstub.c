#define _GNU_SOURCE 1

#include <dev/disk.h>
#include <dev/kclock.h>
#include <dev/picirq.h>
#include <kern/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

int kclock_hz = 100;

void
kclock_delay(int usec)
{
    struct timespec d;
    d.tv_sec = usec / 1000000;
    d.tv_nsec = (usec % 1000000) * 1000;
    nanosleep(&d, 0);
}

uint64_t
kclock_ticks_to_msec(uint64_t ticks)
{
    return ticks * 1000 / kclock_hz;
}

uint16_t irq_mask_8259A;
void
irq_setmask_8259A(uint16_t mask)
{
    printf("irq_setmask_8259A: 0x%x\n", mask);
}

void
irq_eoi(int irqno)
{
    printf("irq_eoi(%d)\n", irqno);
}

void __attribute__((noreturn))
machine_reboot(void)
{
    printf("machine_reboot(): adios\n");
    exit(-1);
}
