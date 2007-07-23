#include <inc/lib.h>
#include <inc/arch.h>
#include <inc/utrap.h>

uint64_t 
arch_read_tsc(void)
{
    return 0;
}

int
utrap_is_masked(void)
{
    return 0;
}

int
utrap_set_mask(int masked)
{
    return 0;
}
