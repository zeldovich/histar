#define _GNU_SOURCE 1

#include <dev/disk.h>
#include <dev/kclock.h>
#include <dev/picirq.h>
#include <dev/console.h>
#include <kern/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

void
ide_intr(void)
{
    printf("Hmm, ide_intr()...\n");
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

struct Thread_list console_waiting;

void
cons_putc(int c)
{
    putc(c, stdout);
}

int
cons_getc(void)
{
    char c;
    int r = recv(0, &c, 1, MSG_DONTWAIT);
    if (r < 0)
	return 0;
    return c;
}

int
cons_probe(void)
{
    return 0;
}

void
cons_cursor(int line, int col)
{
    printf("cons_cursor(%d, %d)\n", line, col);
}
