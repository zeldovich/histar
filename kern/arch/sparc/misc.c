#include <kern/arch.h>
#include <kern/timer.h>
#include <machine/sparc-common.h>

uint64_t
karch_get_tsc(void)
{
    /* XXX */
    return timer_user_nsec();
}

uintptr_t
karch_get_sp(void)
{
    return rd_sp();
}
