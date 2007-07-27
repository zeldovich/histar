#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/asi.h>
#include <machine/sparc-config.h>
#include <machine/memlayout.h>

#define TRAP_ENTRY \
	rd %psr, %l0; rd %wim, %l3; b def_trap; nop;
#define WINDOW_ENTRY(label) \
	rd %psr, %l0; mov %l1, %g5; ba label; andcc %l0, PSR_PS, %g0;

	/* The Trap Base Address (TBA) must be 4096 aligned */
	.text
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

ENTRY(start)
	/* Turn on Supervisor, EnableFloating, and all the PIL bits.
	 * Also puts us in register window zero with traps off.
	 */
	set	(PSR_PS | PSR_S | PSR_PIL | PSR_EF), %g2
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
	set	kstack_top, %g1
	or	%g0, %g1, %sp
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

def_trap:
	set	kstack_top, %l5
	sub	%l5, TRAPFRAME_SZ, %l5

	STORE_GLOBALS(l5 + TF_G0)
	STORE_TRAPFRAME_OTHER(l5, l0, l1, l2, l4)
	mov	%l5, %g7

	restore
	STORE_OUTS(g7 + TF_O0)
	STORE_LOCALS(g7 + TF_L0)
	STORE_INS(g7 + TF_I0)
	/* Have a saved trapframe, so can use any registers */

	/* Make window 1 invalid */
	mov	0x02, %wim
	WRITE_PAUSE
	/* set CWP to 0 */
	rd	%psr, %l0
	andn	%l0, PSR_CWP, %l0
	mov	%l0, %psr
	WRITE_PAUSE

	/* Disable interrupts, enable traps */
	rd	%psr, %l0
	bset	PSR_ET | PSR_PIL, %l0
	mov	%l0, %psr
	WRITE_PAUSE

	mov	%g0, %fp
	mov	%g7, %sp

	rd	%tbr, %o1
	call	trap_handler
	 mov	%sp, %o0

ENTRY(trapframe_pop)
	mov	%o0, %g1
	mov	%g0, %wim
	WRITE_PAUSE
	/* set CWP to 0 */
	rd	%psr, %l0
	andn	%l0, PSR_CWP, %l0
	mov	%l0, %psr
	WRITE_PAUSE
	
	LOAD_INS(g1 + TF_I0)
	LOAD_LOCALS(g1 + TF_L0)
	LOAD_OUTS(g1 + TF_O0)

	save

	mov	%g1, %l0
	LOAD_GLOBALS(l0 + TF_G0)

	ld	[%l0 + TF_PC], %l1
	ld	[%l0 + TF_NPC], %l2

	ld	[%l0 + TF_Y], %l3
	mov	%l3, %y

	/* Make only window 0 valid */
	set	0x0FE, %l3
	mov	%l3, %wim
	WRITE_PAUSE
	/* Disable traps, enable interrupts, clear PS */
	rd	%psr, %l3
	andn	%l3, PSR_ET | PSR_PS | PSR_PIL, %l3
	mov	%l3, %psr
	WRITE_PAUSE
	
	jmp	%l1
	 rett	%l2

/* Window overflow/spill and underflow/fill handlers.
 * After the branch and delay are executed in window_fill/spill,
 * the register state is:
 * %l0 == psr
 * %l1 == %g5 == pc
 * %l2 == %g6 == npc
 * We optimize for user window traps.
 */
window_spill:	
	bz	user_spill
	 mov	%l2, %g6

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
	WRITE_PAUSE

	STORE_WINDOW(sp)

	restore

	mov	%l7, %g1

	jmp	%l1
	 rett	%l2	

user_spill:
	set	UWINOVERFLOW, %g7
	jmp	%g7
	 rett	%g7 + 4
	
window_fill:	
	bz	user_fill
	 mov	%l2, %g6

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
	set	UWINUNDERFLOW, %g7
	jmp	%g7
	 rett	%g7 + 4

.globl	thread_arch_idle_asm
thread_arch_idle_asm:
	set	kstack_top, %sp
	/* enable interrupts */
	rd	%psr, %l3
	andn	%l3, PSR_PIL, %l3
	mov	%l3, %psr
	WRITE_PAUSE
1:	ba	1b
	 nop
