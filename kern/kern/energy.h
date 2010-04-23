#ifndef JOS_KERN_ENERGY_H
#define JOS_KERN_ENERGY_H

#include <machine/types.h>

uint64_t energy_cpu_mW(void)
    __attribute__ ((warn_unused_result));

uint64_t energy_cpu_mJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));

uint64_t energy_hd_mW(void)
    __attribute__ ((warn_unused_result));

uint64_t energy_hd_mJ(uint64_t nsecs)
    __attribute__ ((warn_unused_result));
#endif
