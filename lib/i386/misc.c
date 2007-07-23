#include <inc/lib.h>
#include <inc/arch.h>
#include <machine/x86.h>

uint64_t 
arch_read_tsc(void)
{
    return read_tsc();
}
