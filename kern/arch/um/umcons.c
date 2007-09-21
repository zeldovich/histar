#include <stdio.h>
#include <kern/console.h>
#include <machine/um.h>

static void
um_putchar(void *arg, int c)
{
    putchar(c);
}

void
um_cons_init(void)
{
    static struct cons_device um_cd = { .cd_output = &um_putchar };
    cons_register(&um_cd);
}
