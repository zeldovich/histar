#include <inc/lib.h>
#include <inc/arch.h>
#include <machine/pmap.h>
#include <machine/x86.h>

void utrap_set_cs(uint16_t nval);	/* x86 asm stub */

uint64_t 
arch_read_tsc(void)
{
    return read_tsc();
}

int
arch_utrap_is_masked(void)
{
    return (read_cs() == GD_UT_MASK);
}

int
arch_utrap_set_mask(int masked)
{
    uint16_t old_cs = read_cs();
    uint16_t new_cs = masked ? GD_UT_MASK : GD_UT_NMASK;
    if (old_cs != new_cs)
	utrap_set_cs(new_cs);
    return old_cs == GD_UT_MASK;
}
