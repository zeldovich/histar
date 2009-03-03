#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <inc/intmacro.h>
#include <dev/goldfish_ttycons.h>

static void
goldfish_ttycons_putc(void *arg, int c, cons_source src)
{
	volatile uint32_t *tty = (volatile uint32_t *)0xff002000;

	c &= 0xff;
	*tty = c;
}

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
goldfish_ttycons_proc_data(void *arg)
{
	return (-1);
}

static void
goldfish_ttycons_intr(void *arg)
{
	cons_intr(goldfish_ttycons_proc_data, arg);
}

void
goldfish_ttycons_init(void)
{
	static struct cons_device goldfish_ttycons_cd = {
		.cd_pollin = &goldfish_ttycons_proc_data,
		.cd_output = &goldfish_ttycons_putc,
	};
	static struct interrupt_handler ih = {
		.ih_func = &goldfish_ttycons_intr
	};

	irq_register(4, &ih);

	cons_register(&goldfish_ttycons_cd);
}
