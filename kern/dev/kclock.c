/* See COPYRIGHT for copyright information. */

/* The Run Time Clock and other NVRAM access functions that go with it. */
/* The run time clock is hard-wired to IRQ8. */

int kclock_hz = 1000;
static int kclock_tval;

#include <machine/x86.h>
#include <inc/isareg.h>
#include <inc/timerreg.h>

#include <dev/kclock.h>
#include <dev/picirq.h>
#include <kern/lib.h>

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
  /* initialize 8253 clock to interrupt kclock_hz times/sec */
  outb (TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);

  kclock_tval = TIMER_DIV(kclock_hz);
  outb (IO_TIMER1, kclock_tval % 256);
  outb (IO_TIMER1, kclock_tval / 256);
  cprintf ("Setup timer interrupts via 8259A\n");
  irq_setmask_8259A (irq_mask_8259A & ~(1 << 0));
}

int
kclock_gettick (void)
{
  outb (TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
  int lo = inb (TIMER_CNTR0);
  int hi = inb (TIMER_CNTR0);
  return (hi << 8) | lo;
}

void
kclock_delay (int usec)
{
  int tick_start = kclock_gettick();

  // This obtuse code comes from NetBSD sys/arch/amd64/isa/clock.c
  int t_sec = usec / 1000000;
  int t_usec = usec % 1000000;
  int ticks = t_sec * TIMER_FREQ +
	      t_usec * (TIMER_FREQ / 1000000) +
	      t_usec * ((TIMER_FREQ % 1000000) / 1000) / 1000 +
	      t_usec * (TIMER_FREQ % 1000) / 1000000;

  while (ticks > 0) {
    int tick_now = kclock_gettick();
    if (tick_now > tick_start)
      ticks -= kclock_tval - (tick_now - tick_start);
    else
      ticks -= tick_start - tick_now;
    tick_start = tick_now;
  }
}

uint64_t
kclock_msec_to_ticks (uint64_t msec)
{
    return msec * kclock_hz / 1000;
}
