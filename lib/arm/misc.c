#include <inc/lib.h>
#include <inc/arch.h>
#include <inc/syscall.h>
#include <inc/utrap.h>
#include <inc/debug_gate.h>

uint64_t 
arch_read_tsc(void)
{
	return (0);
}

int
utrap_is_masked(void)
{
	return (sys_self_utrap_is_masked() == UT_MASK);
}

int
utrap_set_mask(int masked)
{
	uint32_t maskval = (masked) ? UT_MASK : UT_NMASK;
	return (sys_self_utrap_set_mask(maskval) == UT_MASK);
}

void
debug_gate_breakpoint(void)
{
}

void
arch_fpregs_save(void *a)
{
}

void
arch_fpregs_restore(void *a)
{
}
