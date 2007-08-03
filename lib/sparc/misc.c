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
    uint32_t g7;
    __asm__ __volatile__("mov %%g7, %0\n\t": "=r"(g7));
    /* We never call this in the UTRAPMASKED section */
    return g7 == UT_MASK;
}

int
utrap_set_mask(int masked)
{
    uint32_t oldg7;
    __asm__ __volatile__("mov %%g7, %0\n\t": "=r"(oldg7));

    if (masked)
	__asm__ __volatile__("mov %0, %%g7\n\t":: "i"(UT_MASK));
    else
	__asm__ __volatile__("mov %0, %%g7\n\t":: "i"(UT_NMASK));
    
    return oldg7 == UT_MASK;
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
