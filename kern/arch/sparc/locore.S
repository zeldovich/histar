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
t_zero:	TRAP_ENTRY
t_tflt:	TRAP_ENTRY			/* Inst. Access Exception        */
t_bins:	TRAP_ENTRY			/* Illegal Instruction           */
t_pins:	TRAP_ENTRY			/* Privileged Instruction        */
t_fpd:	TRAP_ENTRY			/* Floating Point Disabled       */
t_wovf:	WINDOW_ENTRY(window_spill)	/* Window Overflow               */
t_wunf:	WINDOW_ENTRY(window_fill)	/* Window Underflow              */
t_mna:	TRAP_ENTRY			/* Memory Address Not Aligned    */
t_fpe:	TRAP_ENTRY			/* Floating Point Exception      */
t_dflt:	TRAP_ENTRY			/* Data Miss Exception           */
t_tio:	TRAP_ENTRY			/* Tagged Instruction Ovrflw     */
t_wpt:	TRAP_ENTRY			/* Watchpoint Detected           */
t_bad:	TRAP_ENTRY
	TRAP_ENTRY
	TRAP_ENTRY
	TRAP_ENTRY
	TRAP_ENTRY
t_irq1:	TRAP_ENTRY			/* IRQ Software/SBUS Level 1     */
t_irq2:	TRAP_ENTRY		        /* IRQ SBUS Level 2              */
t_irq3:	TRAP_ENTRY			/* IRQ SCSI/DMA/SBUS Level 3     */
t_irq4:	TRAP_ENTRY			/* IRQ Software Level 4          */
t_irq5:	TRAP_ENTRY			/* IRQ SBUS/Ethernet Level 5     */
t_irq6:	TRAP_ENTRY			/* IRQ Software Level 6          */
t_irq7:	TRAP_ENTRY			/* IRQ Video/SBUS Level 5        */
t_irq8:	TRAP_ENTRY			/* IRQ SBUS Level 6              */
t_irq9:	TRAP_ENTRY			/* IRQ SBUS Level 7              */
t_irq10:TRAP_ENTRY			/* IRQ Timer #1 (one we use)     */
t_irq11:TRAP_ENTRY			/* IRQ Floppy Intr.              */
t_irq12:TRAP_ENTRY			/* IRQ Zilog serial chip         */
t_irq13:TRAP_ENTRY			/* IRQ Audio Intr.               */
t_irq14:TRAP_ENTRY			/* IRQ Timer #2                  */
t_nmi:	TRAP_ENTRY
t_racc:	TRAP_ENTRY			/* General Register Access Error */
t_iacce:TRAP_ENTRY			/* Instr Access Error            */
t_bad22:TRAP_ENTRY
	TRAP_ENTRY
t_cpdis:TRAP_ENTRY			/* Co-Processor Disabled         */
t_uflsh:TRAP_ENTRY			/* Unimplemented FLUSH inst.     */
t_bad26:TRAP_ENTRY
	TRAP_ENTRY
t_cpexc:TRAP_ENTRY			/* Co-Processor Exception        */
t_dacce:TRAP_ENTRY			/* Data Access Error             */
t_hwdz:	TRAP_ENTRY			/* Division by zero, you lose... */
t_dserr:TRAP_ENTRY			/* Data Store Error              */
t_daccm:TRAP_ENTRY			/* Data Access MMU-Miss          */
t_bad2d:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad32:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad37:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_iaccm:TRAP_ENTRY                      /* Instr Access MMU-Miss         */
t_bad3d:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad42:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad47:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad4c:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad51:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad56:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad5b:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad60:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad65:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad6a:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad6f:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad74:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad79:TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY TRAP_ENTRY
t_bad7e:TRAP_ENTRY TRAP_ENTRY
t_sys:	TRAP_ENTRY
t_bkpt:	TRAP_ENTRY
t_emuer:TRAP_ENTRY
		
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
