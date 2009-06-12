#include <machine/asm.h>
#include <inc/gccwrap.h>

#define GCC_WRAP(s) ENTRY(_libgcc_wrap_##s) adr r4, s; ldr r4, [r4]; mov pc, r4;
//GCC_WRAPS_COMMON
//GCC_WRAPS_ARCH
#undef GCC_WRAP
