#include <machine/x86.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <dev/isareg.h>
#include <dev/nvram.h>

void
machine_reboot(void)
{
    outb(IO_RTC, NVRAM_RESET);
    outb(IO_RTC + 1, NVRAM_RESET_RST);
    outb(0x92, 0x3);
    panic("reboot did not work");
}

void
abort(void)
{
    for (;;)
	pause_nop();
}
