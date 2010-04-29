#ifndef JOS_KERN_ENERGY_H
#define JOS_KERN_ENERGY_H

#include <machine/types.h>

int64_t battery_full_charge_mJ(void)
    __attribute__ ((warn_unused_result));

int64_t energy_cpu_mW(void)
    __attribute__ ((warn_unused_result));

int64_t energy_cpu_mJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));

int64_t energy_net_send_mJ(uint64_t bytes)
    __attribute__ ((warn_unused_result));

int64_t energy_net_recv_mJ(uint64_t bytes)
    __attribute__ ((warn_unused_result));

int64_t energy_backlight_mW(int level)
    __attribute__ ((warn_unused_result));

int64_t energy_baseline_mW(void)
    __attribute__ ((warn_unused_result));

int64_t energy_baseline_mJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));
#endif
