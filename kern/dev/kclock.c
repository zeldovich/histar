/* See COPYRIGHT for copyright information. */

/* The Run Time Clock and other NVRAM access functions that go with it. */
/* The run time clock is hard-wired to IRQ8. */

#include <machine/x86.h>
#include <inc/isareg.h>
#include <inc/timerreg.h>

#include <dev/kclock.h>
#include <dev/picirq.h>
#include <kern/lib.h>

uint32_t jiffies;

unsigned
mc146818_read (void *sc, unsigned reg)
{
  outb (IO_RTC, reg);
  return inb (IO_RTC + 1);
}

void
mc146818_write (void *sc, unsigned reg, unsigned datum)
{
  outb (IO_RTC, reg);
  outb (IO_RTC + 1, datum);
}

void
kclock_init (void)
{
  /* initialize 8253 clock to interrupt 1000 times/sec */
  outb (TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
  outb (IO_TIMER1, TIMER_DIV (1000) % 256);
  outb (IO_TIMER1, TIMER_DIV (1000) / 256);
  cprintf ("	Setup timer interrupts via 8259A\n");
  irq_setmask_8259A (irq_mask_8259A & ~(1 << 0));
  cprintf ("	unmasked timer interrupt\n");
}
