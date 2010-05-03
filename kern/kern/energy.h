#ifndef JOS_KERN_ENERGY_H
#define JOS_KERN_ENERGY_H

#include <machine/types.h>

int64_t battery_full_charge_uJ(void)
    __attribute__ ((warn_unused_result));

int64_t energy_cpu_uW(void)
    __attribute__ ((warn_unused_result));

int64_t energy_cpu_uJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));

int64_t energy_net_send_uJ(uint64_t bytes)
    __attribute__ ((warn_unused_result));

int64_t energy_net_recv_uJ(uint64_t bytes)
    __attribute__ ((warn_unused_result));

int64_t energy_backlight_uW(int level)
    __attribute__ ((warn_unused_result));

int64_t energy_baseline_uW(void)
    __attribute__ ((warn_unused_result));

int64_t energy_baseline_uJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));
#endif
