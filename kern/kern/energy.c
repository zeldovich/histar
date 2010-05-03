#include <kern/energy.h>

// generic stuff

int64_t
energy_cpu_uJ(uint64_t nsecs)
{
    return energy_cpu_uW() * nsecs / 1000000000;
}

int64_t
energy_baseline_uJ(uint64_t nsecs)
{
    return energy_baseline_uW() * nsecs / 1000000000;
}

// arch-specific stuff

int64_t
battery_full_charge_uJ()
{
    return 15318lu * 1000 * 1000;	// 1150mAh at 3.7v
}

int64_t
energy_cpu_uW()
{
    return 137lu * 1000;
}

int64_t
energy_net_send_uJ(uint64_t bytes) {
    return bytes * 200 * 1000;	// XXX- need real numbers
}

int64_t
energy_net_recv_uJ(uint64_t bytes)
{
    return bytes * 20 * 1000;  // XXX- need real numbers
}

int64_t
energy_backlight_uW(int level)
{
    if (level)
	return 555lu * 1000;
    return 0;
}

int64_t
energy_baseline_uW()
{
    return 699lu * 1000;			// backlight off
}
