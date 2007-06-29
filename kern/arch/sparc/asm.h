#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

#define TRAP_ENTRY(label) \
        rd %psr, %l0; rd %wim, %l1; b label; nop;
