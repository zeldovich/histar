#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/asi.h>
#include <machine/sparc-config.h>
#include <machine/memlayout.h>
#include <machine/pmap.h>

	.text
	.align 4096

ENTRY(abort)
	/*
	 * Set full %wim and turn off traps to trigger a CPU error later.
	 */
	mov	0xff, %wim
	set	PSR_S, %g2
	wr	%g2, %psr

	/*
	 * Disable the MMU, to simplify re-loading a new kernel with grmon.
	 * This function is also located at 0x80001000, making it easy to
	 * reset the MMU using "run 0x80001000".
	 */
	lda	[%g0] ASI_MMUREGS, %g3
	andn	%g3, SRMMU_CTRL_E, %g3
	sta	%g3, [%g0] ASI_MMUREGS

1:	ba	1b
	 save

/*
 * Bootstrapping.
 */

ENTRY(start)
	/* Turn on Supervisor, EnableFloating, and all the PIL bits.
	 * Also puts us in register window zero with traps off.
	 */
	set	(PSR_S | PSR_PIL | PSR_EF), %g2
	wr	%g2, %psr
	WRITE_PAUSE
		
	/* Set the first PTD of the Context Table to point to bootpt */
	set	RELOC(bootpt), %g2 
	srl	%g2, 4, %g2
	or	%g2, PT_ET_PTD, %g2
	set	RELOC(bootct), %g1
	st	%g2, [%g1]

	/* Set Context Table Pointer register */	
	srl	%g1, 4, %g1
	set	SRMMU_CTXTBL_PTR, %g2
	sta	%g1, [%g2] ASI_MMUREGS

	/* Set context register to 0 */
	set	SRMMU_CTX_REG, %g2
	sta	%g0, [%g2] ASI_MMUREGS

	/* Enable MMU */
	lda	[%g0] ASI_MMUREGS, %g3
	or	%g3, SRMMU_CTRL_E, %g3
	sta	%g3, [%g0] ASI_MMUREGS
	WRITE_PAUSE

	set	vm_enabled, %g1
	jmpl	%g1, %g0
	 nop

vm_enabled:	
	/* Make window 1 invalid */
	mov	0x02, %wim
	WRITE_PAUSE

	/* kernel stack */
	set	(kstack_top - STACKFRAME_SZ), %sp
	mov	0, %fp

	/* Start using the trap table  */
	set	trapbase, %g3
	mov	%g3, %tbr

	/* Turn on traps so that we can call c-code. */	
	rd	%psr, %g3
	bset	PSR_ET, %g3
	mov	%g3, %psr
	WRITE_PAUSE

	call	init
	 nop

/*
 * Trap handling.
 */

#define TRAP_ENTRY \
	rd %psr, %l0; rd %wim, %l3; b def_trap; nop;
#define WINDOW_ENTRY(label) \
	rd %psr, %l0; mov %l1, %y; ba label; andcc %l0, PSR_PS, %g0;

	/* The Trap Base Address (TBA) must be 4096 aligned */
	.align 4096
trapbase:
	.rept 5				/* Traps 0 - 4 */
	TRAP_ENTRY
	.endr
t_wovf:	WINDOW_ENTRY(window_spill)	/* 5: Window Overflow */
t_wunf:	WINDOW_ENTRY(window_fill)	/* 6: Window Underflow */
	.rept 249			/* Traps 7 - 255 */
	TRAP_ENTRY
	.endr

def_trap:
	set	(kstack_top - TRAPFRAME_SZ), %l5

	STORE_REG_SET(g, l5 + TF_G0)

	st	%l0, [%l5 + TF_PSR]
	st	%l1, [%l5 + TF_PC]
	st	%l2, [%l5 + TF_NPC]
	st	%l3, [%l5 + TF_WIM]
	rd	%y, %l4
	st	%l4, [%l5 + TF_Y]

	mov	%l5, %g7

	restore
	STORE_REG_SET(o, g7 + TF_O0)
	STORE_REG_SET(l, g7 + TF_L0)
	STORE_REG_SET(i, g7 + TF_I0)
	/* Have a saved trapframe, so can use any registers */

	/* Make window 1 invalid */
	mov	0x02, %wim

	/* set CWP to 0, disable interrupts, enable traps */
	rd	%psr, %l0
	andn	%l0, PSR_CWP, %l0
	bset	PSR_ET | PSR_PIL, %l0
	mov	%l0, %psr
	WRITE_PAUSE

	mov	%g0, %fp
	mov	%g7, %o0
	mov	%g7, %sp

	rd	%tbr, %o1
	call	trap_handler
	 sub	%sp, STACKFRAME_SZ, %sp

ENTRY(trapframe_pop)
	mov	%o0, %g1
	mov	%g0, %wim
	WRITE_PAUSE

	ld	[%g1 + TF_PSR], %l0
	mov	%l0, %psr
	WRITE_PAUSE

	/* move to window we want to reload */
	restore

	LOAD_REG_SET(i, g1 + TF_I0)
	LOAD_REG_SET(l, g1 + TF_L0)
	LOAD_REG_SET(o, g1 + TF_O0)

	save

	mov	%g1, %l0
	LOAD_REG_SET(g, l0 + TF_G0)

	ld	[%l0 + TF_WIM], %l3
	mov	%l3, %wim

	ld	[%l0 + TF_PC], %l1
	ld	[%l0 + TF_NPC], %l2
	ld	[%l0 + TF_Y], %l3

	mov	%l3, %y
	jmp	%l1
	 rett	%l2

ENTRY(thread_arch_idle_asm)
	/* enable interrupts */
	rd	%psr, %l3
	andn	%l3, PSR_PIL, %l3
	mov	%l3, %psr
1:	ba	1b
	 nop

/* Window overflow/spill and underflow/fill handlers.
 * After the branch and delay are executed in window_fill/spill,
 * the register state is:
 * %l0 == psr
 * %l1 == %y == pc
 * %l2 == %g5 == npc
 * We optimize for user window traps.  We use %y instead of %g1,
 * because it seems GCC assumes %g1 is preserved across function
 * calls in some places.
 */
window_spill:	
	bz	user_spill
	 mov	%l2, %g5

kernel_spill:
	mov	%g1, %l7

	/* rotate WIM bit right */
	mov	%wim, %l3
	sll	%l3, NWINDOWS - 1, %l4
	srl	%l3, 1, %l3
	or	%l3, %l4, %g1

        /* CWP to window we are going to trap onto next */
	save
	mov	%g1, %wim

	STORE_WINDOW(sp)
	restore

	mov	%l7, %g1

	jmp	%l1
	 rett	%l2	

user_spill:
	set	UWINOVERFLOW, %l7
	jmp	%l7
	 rett	%l7 + 4

window_fill:	
	bz	user_fill
	 mov	%l2, %g5

kernel_fill:
        /* rotate WIM bit left */
        mov	%wim,	%l3
        srl	%l3, NWINDOWS - 1, %l4
        sll	%l3, 1, %l3
        or	%l3, %l4, %l3

	mov	%l3, %wim
	WRITE_PAUSE

	restore
	restore

	LOAD_WINDOW(sp)

	save
	save

	jmp	%l1
	 rett	%l2

user_fill:
	set	UWINUNDERFLOW, %l7
	jmp	%l7
	 rett	%l7 + 4

