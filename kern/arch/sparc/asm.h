#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

/* Have to do this after %wim %psr change */
#define WRITE_PAUSE nop; nop; nop;

/* Store the register window onto the 8-byte aligned area starting
 * at %reg.  It might be %sp, it might not, we don't care.
 */
#define STORE_WINDOW(reg)			\
        std     %l0, [%reg + RW_L0];		\
        std     %l2, [%reg + RW_L2];		\
        std     %l4, [%reg + RW_L4];		\
        std     %l6, [%reg + RW_L6];		\
        std     %i0, [%reg + RW_I0];		\
        std     %i2, [%reg + RW_I2];		\
        std     %i4, [%reg + RW_I4];		\
        std     %i6, [%reg + RW_I6];

/* Load a register window from the area beginning at %reg. */
#define LOAD_WINDOW(reg)			\
        ldd     [%reg + RW_L0], %l0;		\
        ldd     [%reg + RW_L2], %l2;		\
        ldd     [%reg + RW_L4], %l4;		\
        ldd     [%reg + RW_L6], %l6;		\
        ldd     [%reg + RW_I0], %i0;		\
        ldd     [%reg + RW_I2], %i2;		\
        ldd     [%reg + RW_I4], %i4;		\
        ldd     [%reg + RW_I6], %i6;

/* Store/load register sets onto the 8-byte aligned area */
#define STORE_REG_SET(pfx, base_reg)		\
	std	%pfx##0, [%base_reg + 0];	\
	std	%pfx##2, [%base_reg + 8];	\
	std	%pfx##4, [%base_reg + 16];	\
	std	%pfx##6, [%base_reg + 24];

#define LOAD_REG_SET(pfx, base_reg)		\
	ldd	[%base_reg + 0], %pfx##0;	\
	ldd	[%base_reg + 8], %pfx##2;	\
	ldd	[%base_reg + 16], %pfx##4;	\
	ldd	[%base_reg + 24], %pfx##6;

#define _SV save	%sp, -STACKFRAME_SZ, %sp
#define _RS restore
#define FLUSH_WINDOWS \
        _SV; _SV; _SV; _SV; _SV; _SV; _SV; _SV; \
        _RS; _RS; _RS; _RS; _RS; _RS; _RS; _RS;
