#include <machine/x86.h>
#include <kern/arch.h>

void
machine_reboot(void)
{
    outb(0x92, 0x3);
}
