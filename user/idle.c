#include <inc/syscall.h>
#include <machine/trapcodes.h>

int
main(int ac, char **av)
{
    // XXX
    //
    // the assembly implementation is necessary to be able to
    // call sys_thread_yield() without requiring a writable
    // page (namely the stack) during pstate swapout..
    //
    // once we get real copy-on-write there, this can go away.

    __asm__ __volatile__(
	"movq %1, %%rdi\n"
	"1:\n"
	"int %0\n"
	"jmp 1b\n"

	: // no output
	: "n" (T_SYSCALL),
	  "n" (SYS_thread_yield)
	);

    for (;;)
	sys_thread_yield();
}
