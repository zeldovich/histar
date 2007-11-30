#include <machine/asm.h>
#include <inc/gccwrap.h>

#define GCC_WRAP(x) ENTRY(x) jmp _libgcc_wrap_##x;
GCC_WRAPS_COMMON
GCC_WRAPS_ARCH
#undef GCC_WRAP

