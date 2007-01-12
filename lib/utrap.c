#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inc/memlayout.h>
#include <stddef.h>
#include <string.h>

/* x86 %cs-based masking */
#include <machine/pmap.h>
#include <machine/x86.h>

static void (*handler) (struct UTrapframe *);

void
utrap_set_handler(void (*fn) (struct UTrapframe *))
{
    handler = fn;
}

void
utrap_entry(struct UTrapframe *utf)
{
    if (handler) {
	handler(utf);
    } else {
	cprintf("utrap_entry: unhandled trap src %d num %d arg 0x%lx\n",
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
utrap_set_mask(int masked)
{
    uint16_t old_cs = read_cs();
    uint16_t new_cs = masked ? GD_UT_MASK : GD_UT_NMASK;
    if (old_cs != new_cs)
	utrap_set_cs(new_cs);
    return old_cs == GD_UT_MASK;
}

static void __attribute__((used))
utrap_field_symbols(void)
{
#define UTF_DEF(field)							\
    __asm volatile (".globl\t" #field "\n\t.set\t" #field ",%0"		\
		    :: "m" (*(int *) offsetof (struct UTrapframe, field)))

    UTF_DEF (utf_rax);
    UTF_DEF (utf_rbx);
    UTF_DEF (utf_rcx);
    UTF_DEF (utf_rdx);

    UTF_DEF (utf_rsi);
    UTF_DEF (utf_rdi);
    UTF_DEF (utf_rbp);
    UTF_DEF (utf_rsp);

    UTF_DEF (utf_r8);
    UTF_DEF (utf_r9);
    UTF_DEF (utf_r10);
    UTF_DEF (utf_r11);

    UTF_DEF (utf_r12);
    UTF_DEF (utf_r13);
    UTF_DEF (utf_r14);
    UTF_DEF (utf_r15);

    UTF_DEF (utf_rip);
    UTF_DEF (utf_rflags);
}
