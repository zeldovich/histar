#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inc/memlayout.h>
#include <inc/arch.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

static void (*handler) (struct UTrapframe *);

void
utrap_set_handler(void (*fn) (struct UTrapframe *))
{
    handler = fn;
}

void __attribute__((noreturn, JOS_UTRAP_GCCATTR))
utrap_entry(struct UTrapframe *utf)
{
    if (handler) {
	handler(utf);
    } else {
	cprintf("utrap_entry: unhandled trap src %d num %d arg 0x%"PRIx64"\n",
		utf->utf_trap_src, utf->utf_trap_num, utf->utf_trap_arg);
	sys_self_halt();
    }

    utrap_ret(utf);
}

int
utrap_init(void)
{
    int r = segment_set_utrap(&utrap_entry_asm, tls_base, tls_stack_top);
    if (r < 0) {
	cprintf("utrap_init: cannot set trap entry: %s\n", e2s(r));
	return r;
    }

    return 0;
}
