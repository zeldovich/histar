#include <machine/x86.h>
#include <kern/arch.h>

uint64_t
karch_get_tsc(void)
{
    return read_tsc();
}
