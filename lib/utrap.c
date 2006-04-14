#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inc/memlayout.h>
#include <stddef.h>
#include <string.h>

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
    int xstack_pages = 2;

    void *utrap_stack = (void *) UTRAPSTACKTOP - xstack_pages * PGSIZE;
    uint64_t nbytes = (xstack_pages + 1) * PGSIZE;
    struct cobj_ref o;
    int r = segment_alloc(start_env->proc_container, nbytes,
			  &o, &utrap_stack, 0, "trap stack");
    if (r < 0)
	return r;

    void *utrap_code = (void *) UTRAPHANDLER - xstack_pages * PGSIZE;
    r = segment_map(o, SEGMAP_READ | SEGMAP_EXEC, &utrap_code, &nbytes);
    if (r < 0)
	return r;

    uint32_t utrap_stub_size = (uint32_t) ((void *) utrap_stub_end -
					   (void *) utrap_stub);
    memcpy(utrap_stack + xstack_pages * PGSIZE, (void *) utrap_stub,
	   utrap_stub_size);
    return 0;
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
