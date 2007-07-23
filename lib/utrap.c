#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inc/memlayout.h>
#include <inc/arch.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

/* x86 %cs-based masking */
#include <machine/pmap.h>
#include <machine/x86.h>

static void (*handler) (struct UTrapframe *);

void
utrap_set_handler(void (*fn) (struct UTrapframe *))
{
    handler = fn;
}

void __attribute__((regparm (1)))
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

int
utrap_is_masked(void)
{
    return arch_utrap_is_masked();
}

int
utrap_set_mask(int masked)
{
    return arch_utrap_set_mask(masked);
}
