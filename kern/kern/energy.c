#include <kern/energy.h>

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
energy_cpu_mJ(uint64_t nsecs)
{
    return energy_cpu_mW() * nsecs / 1000000000;
}

int64_t
energy_net_mJ(uint64_t bytes)
{
    return bytes * 20;	// XXX- get number from arjun. may need to add overhead
}

int64_t
energy_baseline_mW()
{
    return 87300;
}

int64_t
energy_baseline_mJ(uint64_t nsecs)
{
    return energy_baseline_mW() * nsecs / 1000000000;
}
