#include <dev/leonsercons.h>
#include <kern/console.h>

static void
serial_putc(void *x, int c)
{
    
}

void
leonsercons_init(void)
{
    static struct cons_device cd = {
	.cd_pollin = 0,
	.cd_output = &serial_putc,
    };
    cons_register(&cd);
}
