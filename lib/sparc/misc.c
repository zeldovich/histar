#include <inc/lib.h>
#include <inc/arch.h>
#include <inc/utrap.h>
#include <inc/debug_gate.h>

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
