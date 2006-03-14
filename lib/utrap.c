#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/utrap.h>
#include <inc/memlayout.h>
#include <stddef.h>
#include <string.h>

void
utrap_entry(struct UTrapframe *utf)
{
    cprintf("utrap_entry..\n");

    sys_self_halt();
    utrap_ret(utf);
}

int
utrap_init(void)
{
    void *utrap_stack = (void *) UTRAPSTACKTOP - PGSIZE;
    struct cobj_ref o;
    int r = segment_alloc(start_env->proc_container, PGSIZE,
			  &o, &utrap_stack, 0, "trap stack");
    if (r < 0)
	return r;

    r = segment_alloc(start_env->proc_container, PGSIZE, &o, 0, 0, "trap code");
    if (r < 0)
	return r;

    void *utrap_code = (void *) UTRAPHANDLER;
    r = segment_map(o, SEGMAP_READ | SEGMAP_WRITE | SEGMAP_EXEC, &utrap_code, 0);
    if (r < 0)
	return r;

    uint32_t utrap_stub_size = (uint32_t) ((void *) utrap_stub_end -
					   (void *) utrap_stub);
    memcpy(utrap_code, (void *) utrap_stub, utrap_stub_size);
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
