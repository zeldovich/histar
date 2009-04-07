#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <inc/intmacro.h>
#include <dev/goldfish_ttycons.h>
#include <dev/goldfish_ttyconsreg.h>

#define GF_TTY_DEVICE	((struct goldfish_ttycons_reg *)0xff002000)

static void
goldfish_ttycons_putc(void *arg, int c, cons_source src)
{
	c &= 0xff;
	GF_TTY_DEVICE->put_char = c;
}

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
goldfish_ttycons_proc_data(void *arg)
{
	char buf;

	if (GF_TTY_DEVICE->bytes_ready == 0)
		return (-1);

	GF_TTY_DEVICE->data_ptr = (uint32_t)&buf;  //yes, emulator does kva->pa
	GF_TTY_DEVICE->data_len = 1;
	GF_TTY_DEVICE->command = GF_TTY_CMD_READ_BUF;

	/* result is instantaneous (?) */
	return (buf & 0x7f);
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

	// enable interrupts
	GF_TTY_DEVICE->command = GF_TTY_CMD_INT_ENABLE;

	irq_register(4, &ih);

	cons_register(&goldfish_ttycons_cd);
}
