#include <kern/energy.h>

uint64_t
energy_cpu_mW()
{
    return 1000;
}

uint64_t
energy_cpu_mJ(uint64_t nsecs)
{
    return energy_cpu_mW() * nsecs / 1000000000;
}

uint64_t
energy_hd_mW()
{
    return 5000;
}

uint64_t
energy_hd_mJ(uint64_t nsecs)
{
    return energy_hd_mW() * nsecs / 1000000000;
}
