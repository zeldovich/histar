#include <inc/lib.h>
#include <inc/utrap.h>
#include <stddef.h>

static void __attribute__((used))
utrap_field_symbols(void)
{
#define GEN_DEF(n, v)  __asm volatile("#define " #n " %0" :: "m" (*(int *) (v)))
#define UTF_DEF(field) GEN_DEF(field, offsetof (struct UTrapframe, field))

#ifdef JOS_ARCH_amd64
    UTF_DEF(utf_rax);
    UTF_DEF(utf_rbx);
    UTF_DEF(utf_rcx);
    UTF_DEF(utf_rdx);

    UTF_DEF(utf_rsi);
    UTF_DEF(utf_rdi);
    UTF_DEF(utf_rbp);
    UTF_DEF(utf_rsp);

    UTF_DEF(utf_r8);
    UTF_DEF(utf_r9);
    UTF_DEF(utf_r10);
    UTF_DEF(utf_r11);

    UTF_DEF(utf_r12);
    UTF_DEF(utf_r13);
    UTF_DEF(utf_r14);
    UTF_DEF(utf_r15);

    UTF_DEF(utf_rip);
    UTF_DEF(utf_rflags);
#endif

#ifdef JOS_ARCH_i386
    UTF_DEF(utf_eax);
    UTF_DEF(utf_ebx);
    UTF_DEF(utf_ecx);
    UTF_DEF(utf_edx);

    UTF_DEF(utf_esi);
    UTF_DEF(utf_edi);
    UTF_DEF(utf_ebp);
    UTF_DEF(utf_esp);

    UTF_DEF(utf_eip);
    UTF_DEF(utf_eflags);

    GEN_DEF(tls_utrap_ret_buf, &TLS_DATA->tls_utrap_ret_buf);
#endif
}
