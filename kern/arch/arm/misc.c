#include <kern/arch.h>
#include <kern/lib.h>

int raise(int sig);

int __attribute__((noreturn))
raise(int sig)
{
    panic("raise %d", sig);
}

void
abort(void)
{
    for (;;)
	;
}

uintptr_t
karch_get_sp(void)
{
    char x;
    uintptr_t sp = (uintptr_t) &x;
    return sp;
}

uint64_t
karch_get_tsc(void)
{
    return 0;
}

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    cprintf("%s\n", __func__);
}

void
karch_fp_init(struct Fpregs *fpreg)
{
    cprintf("%s\n", __func__);
}

void
machine_reboot(void)
{
    panic("no idea how to reboot");
}

void
irq_arch_enable(uint32_t irqno)
{
    cprintf("%s\n", __func__);
}
