#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

/* Have to do this after %wim %psr change */
#define WRITE_PAUSE \
        nop; nop; nop;
