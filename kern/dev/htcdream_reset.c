#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/console.h>
#include <dev/msm_gpio.h>
#include <dev/htcdream_reset.h>

static void __attribute__((__noreturn__))
htcdream_reset()
{
	while (1)
		msm_gpio_write(25, 0);
}

void
htcdream_reset_init()
{
	reboot_hook = htcdream_reset;
}
