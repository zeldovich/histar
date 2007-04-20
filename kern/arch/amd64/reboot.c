#include <machine/x86.h>
#include <kern/arch.h>
#include <kern/lib.h>

void
machine_reboot(void)
{
    outb(0x92, 0x3);
}

void
abort(void)
{
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8AE0);
    for (;;)
	;
}
