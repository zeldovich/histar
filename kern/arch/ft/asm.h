
#ifndef _ALIGN_TEXT
#define _ALIGN_TEXT .align 16, 0x90
#endif

#define ENTRY(x) \
        .text; _ALIGN_TEXT; .globl x; .type x,@function; x:
