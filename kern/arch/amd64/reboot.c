#include <machine/x86.h>
#include <kern/arch.h>
#include <kern/lib.h>

void
machine_reboot(void)
{
    outb(0x92, 0x3);
    abort();
}

void
abort(void)
{
    for (;;)
	;
}
