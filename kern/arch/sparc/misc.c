#include <kern/arch.h>
#include <kern/timer.h>
#include <kern/lib.h>
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

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    jb->jb_sp = (uint32_t) ROUNDUP(stackbase, PGSIZE) - STACKFRAME_SZ;
    /* longjmp uses retl (jmpl %o7 + 8, %g0) */
    jb->jb_pc = (uint32_t) fn - 8;
}

void __attribute__((noreturn))
karch_fp_init(struct Fpregs *fpreg)
{
    panic("no fp support");
}
