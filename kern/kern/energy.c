#include <kern/energy.h>

// generic stuff

int64_t
energy_cpu_mJ(uint64_t nsecs)
{
    return energy_cpu_mW() * nsecs / 1000000000;
}

int64_t
energy_baseline_mJ(uint64_t nsecs)
{
    return energy_baseline_mW() * nsecs / 1000000000;
}

// arch-specific stuff

#ifdef JOS_ARCH_arm

int64_t
battery_full_charge_mJ()
{
    return 15318 * 1000;	// 1150mAh at 3.7v
}

int64_t
energy_cpu_mW()
{
    return 137;
}

int64_t
energy_net_send_mJ(uint64_t bytes) {
    return bytes * 200;	// XXX- need real numbers
}

int64_t
energy_net_recv_mJ(uint64_t bytes)
{
    return bytes * 20;  // XXX- need real numbers
}

int64_t
energy_backlight_mW(int level)
{
    if (level)
	return 555;
    return 0;
}

int64_t
energy_baseline_mW()
{
    return 699;			// backlight off
}

#else

int64_t
battery_full_charge_mJ()
{
    return 1885680000;
}

int64_t
energy_cpu_mW()
{
    return 12000;
}

int64_t
energy_net_send_mJ(uint64_t bytes)
{
    return bytes * 20;	// XXX- no idea
}

int64_t
energy_net_recv_mJ(uint64_t bytes)
{
    return bytes * 20;	// XXX- no idea
}

int64_t
energy_backlight_mW(int level)
{
    return 0;
}

int64_t
energy_baseline_mW()
{
    return 87300;
}

#endif
