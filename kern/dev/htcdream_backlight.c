#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/reserve.h>
#include <kern/energy.h>
#include <dev/htcdream_backlight.h>
#include <dev/msm_gpio.h>

static int current_level;
static uint64_t last_time_nsec;

// this is fine just to poll. the only control for backlight is via
// a button press read by the kernel
static void
htcdream_backlight_bill()
{
	uint64_t now = timer_user_nsec();
	uint64_t diff_us = (now - last_time_nsec) / 1000;

	int64_t uW = energy_backlight_uW(current_level);
	reserve_consume(root_rs, (diff_us * uW) / (1000 * 1000), 1);

	last_time_nsec = now;
}

void
htcdream_backlight_init()
{
	current_level = 100;
	last_time_nsec = timer_user_nsec();
	msm_gpio_write(27, 1);

	static struct periodic_task htcdream_backlight_timer;
	htcdream_backlight_timer.pt_interval_msec = 250;
	htcdream_backlight_timer.pt_fn = htcdream_backlight_bill;
	timer_add_periodic(&htcdream_backlight_timer);
}

void
htcdream_backlight_level(int level)
{
	if (level == 0)
		msm_gpio_write(27, 0);
	else
		msm_gpio_write(27, 1);

	current_level = level;
}
