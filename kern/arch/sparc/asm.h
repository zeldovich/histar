#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

/* Have to do this after %wim %psr change */
#define WRITE_PAUSE \
        nop; nop; nop;

#define SAVE_ALL \
        sethi   %hi(trap_setup), %l4; \
        jmpl    %l4 + %lo(trap_setup), %l5; \
         nop;

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

#define STORE_GLOBALS(base_reg) \
        std     %g0, [%base_reg + TF_G0]; \
        std     %g2, [%base_reg + TF_G2]; \
        std     %g4, [%base_reg + TF_G4]; \
        std     %g6, [%base_reg + TF_G6];

#define STORE_OUTS(base_reg) \
        std     %o0, [%base_reg + TF_O0]; \
        std     %o2, [%base_reg + TF_O2]; \
        std     %o4, [%base_reg + TF_O4]; \
        std     %o6, [%base_reg + TF_O6];

#define STORE_LOCALS(base_reg) \
        std     %l0, [%base_reg + TF_L0]; \
        std     %l2, [%base_reg + TF_L2]; \
        std     %l4, [%base_reg + TF_L4]; \
        std     %l6, [%base_reg + TF_L6];

#define STORE_INS(base_reg) \
        std     %i0, [%base_reg + TF_I0]; \
        std     %i2, [%base_reg + TF_I2]; \
        std     %i4, [%base_reg + TF_I4]; \
        std     %i6, [%base_reg + TF_I6];

#define STORE_TRAPFRAME_OTHER(base_reg, reg_psr, reg_pc, reg_npc, g_scratch) \
        st      %reg_psr, [%base_reg + TF_PSR]; \
        st      %reg_pc,  [%base_reg + TF_PC]; \
        st      %reg_npc, [%base_reg + TF_NPC]; \
        rd      %y, %g_scratch; \
        st      %g_scratch, [%base_reg + TF_Y];

#define STORE_TRAPFRAME_REGFILE(base_reg) \
        STORE_GLOBALS(base_reg) \
        STORE_OUTS(base_reg) \
        STORE_LOCALS(base_reg) \
        STORE_INS(base_reg)

/* Computes a new psr similiar to rett, but without incrementing the
 * CWP.  The old psr is read from psr_reg.
*/
#define RETT_PSR(psr_reg, scratch) \
	or	%psr_reg, PSR_ET, %psr_reg; \
	and	%psr_reg, PSR_PS, %scratch; \
	sll	%scratch, 0x01, %scratch; \
	andn	%psr_reg, (PSR_PS | PSR_S), %psr_reg; \
	or	%scratch, %psr_reg, %psr_reg; 

