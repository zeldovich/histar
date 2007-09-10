#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

/* Have to do this after %wim %psr change */
#define WRITE_PAUSE \
        nop; nop; nop;

/* Store the register window onto the 8-byte aligned area starting
 * at %reg.  It might be %sp, it might not, we don't care.
 */
#define STORE_WINDOW(reg) \
        std     %l0, [%reg + RW_L0]; \
        std     %l2, [%reg + RW_L2]; \
        std     %l4, [%reg + RW_L4]; \
        std     %l6, [%reg + RW_L6]; \
        std     %i0, [%reg + RW_I0]; \
        std     %i2, [%reg + RW_I2]; \
        std     %i4, [%reg + RW_I4]; \
        std     %i6, [%reg + RW_I6];

/* Load a register window from the area beginning at %reg. */
#define LOAD_WINDOW(reg) \
        ldd     [%reg + RW_L0], %l0; \
        ldd     [%reg + RW_L2], %l2; \
        ldd     [%reg + RW_L4], %l4; \
        ldd     [%reg + RW_L6], %l6; \
        ldd     [%reg + RW_I0], %i0; \
        ldd     [%reg + RW_I2], %i2; \
        ldd     [%reg + RW_I4], %i4; \
        ldd     [%reg + RW_I6], %i6;

/* Store/load register sets onto the 8-byte aligned area */
#define STORE_GLOBALS(base_reg) \
        std     %g0, [%base_reg + 0]; \
        std     %g2, [%base_reg + 8]; \
        std     %g4, [%base_reg + 16]; \
        std     %g6, [%base_reg + 24];

#define STORE_OUTS(base_reg) \
        std     %o0, [%base_reg]; \
        std     %o2, [%base_reg + 8]; \
        std     %o4, [%base_reg + 16]; \
        std     %o6, [%base_reg + 24];

#define STORE_LOCALS(base_reg) \
        std     %l0, [%base_reg]; \
        std     %l2, [%base_reg + 8]; \
        std     %l4, [%base_reg + 16]; \
        std     %l6, [%base_reg + 24];

#define STORE_INS(base_reg) \
        std     %i0, [%base_reg]; \
        std     %i2, [%base_reg + 8]; \
        std     %i4, [%base_reg + 16]; \
        std     %i6, [%base_reg + 24];

#define LOAD_GLOBALS(base_reg) \
        ld      [%base_reg + 4], %g1; \
        ldd     [%base_reg + 8], %g2; \
        ldd     [%base_reg + 16], %g4; \
        ldd     [%base_reg + 24], %g6;

#define LOAD_INS(base_reg) \
        ldd     [%base_reg], %i0; \
        ldd     [%base_reg + 8], %i2; \
        ldd     [%base_reg + 16], %i4; \
        ldd     [%base_reg + 24], %i6;

#define LOAD_LOCALS(base_reg) \
        ldd     [%base_reg], %l0; \
        ldd     [%base_reg + 8], %l2; \
        ldd     [%base_reg + 16], %l4; \
        ldd     [%base_reg + 24], %l6;

#define LOAD_OUTS(base_reg) \
        ldd     [%base_reg], %o0; \
        ldd     [%base_reg + 8], %o2; \
        ldd     [%base_reg + 16], %o4; \
        ldd     [%base_reg + 24], %o6;

#define _SV save	%sp, -STACKFRAME_SZ, %sp
#define _RS restore
#define FLUSH_WINDOWS \
        _SV; _SV; _SV; _SV; _SV; _SV; _SV; _SV; \
        _RS; _RS; _RS; _RS; _RS; _RS; _RS; _RS;
