#include <machine/x86.h>
#include <kern/arch.h>

uint64_t
karch_get_tsc(void)
{
    return read_tsc();
}

void
karch_fp_init(struct Fpregs *fpreg)
{
    // Linux says so.
    memset(fpreg, 0, sizeof(*fpreg));
    fpreg->cwd = 0x37f;
    fpreg->mxcsr = 0x1f80;
}
