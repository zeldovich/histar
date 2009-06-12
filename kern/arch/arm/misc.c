#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/timer.h>

void __attribute__((__noreturn__)) (*reboot_hook)(void) = NULL;

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
	if (the_timesrc)
		return the_timesrc->ticks(the_timesrc->arg);

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
	if (reboot_hook != NULL)
		reboot_hook();
	panic("no idea how to reboot");
}
