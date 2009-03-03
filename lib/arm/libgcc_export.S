#include <machine/asm.h>
#include <inc/gccwrap.h>

#define GCC_WRAP(s) ENTRY(_libgcc_wrap_##s) mov pc, s;
GCC_WRAPS_COMMON
GCC_WRAPS_ARCH
#undef GCC_WRAP
