#include <kern/arch.h>
#include <kern/lib.h>

void
machine_reboot(void)
{
    cprintf("No idea how to reboot.\n");
}

void
abort(void)
{
    for (;;)
	;
}
