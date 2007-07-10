#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/asi.h>
#include <machine/srmmu.h>
#include <machine/memlayout.h>
	
#define TRAP_ENTRY_INTERRUPT(num) \
	rd %psr, %l0; rd %wim, %l3; b def_trap; nop;
#define TRAP_ENTRY(label) \
        rd %psr, %l0; rd %wim, %l3; b label; nop;
		
	/* The Trap Base Address (TBA) must be 4096 aligned */
	.text
	.align 4096
trapbase:
t_zero:	TRAP_ENTRY(def_trap)
t_tflt:	TRAP_ENTRY(def_trap)		/* Inst. Access Exception        */
t_bins:	TRAP_ENTRY(def_trap)		/* Illegal Instruction           */
t_pins:	TRAP_ENTRY(def_trap)		/* Privileged Instruction        */
t_fpd:	TRAP_ENTRY(def_trap)		/* Floating Point Disabled       */
t_wovf:	TRAP_ENTRY(window_spill)	/* Window Overflow               */
t_wunf:	TRAP_ENTRY(window_fill)		/* Window Underflow              */
t_mna:	TRAP_ENTRY(def_trap)		/* Memory Address Not Aligned    */
t_fpe:	TRAP_ENTRY(def_trap)		/* Floating Point Exception      */
t_dflt:	TRAP_ENTRY(def_trap)		/* Data Miss Exception           */
t_tio:	TRAP_ENTRY(def_trap)		/* Tagged Instruction Ovrflw     */
t_wpt:	TRAP_ENTRY(def_trap)		/* Watchpoint Detected           */
t_bad:	TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
t_irq1:	TRAP_ENTRY_INTERRUPT(1)         /* IRQ Software/SBUS Level 1     */
t_irq2:	TRAP_ENTRY_INTERRUPT(2)         /* IRQ SBUS Level 2              */
t_irq3:	TRAP_ENTRY_INTERRUPT(3)         /* IRQ SCSI/DMA/SBUS Level 3     */
t_irq4:	TRAP_ENTRY_INTERRUPT(4)         /* IRQ Software Level 4          */
t_irq5:	TRAP_ENTRY_INTERRUPT(5)         /* IRQ SBUS/Ethernet Level 5     */
t_irq6:	TRAP_ENTRY_INTERRUPT(6)         /* IRQ Software Level 6          */
t_irq7:	TRAP_ENTRY_INTERRUPT(7)         /* IRQ Video/SBUS Level 5        */
t_irq8:	TRAP_ENTRY_INTERRUPT(8)         /* IRQ SBUS Level 6              */
t_irq9:	TRAP_ENTRY_INTERRUPT(9)         /* IRQ SBUS Level 7              */
t_irq10:TRAP_ENTRY_INTERRUPT(10)        /* IRQ Timer #1 (one we use)     */
t_irq11:TRAP_ENTRY_INTERRUPT(11)        /* IRQ Floppy Intr.              */
t_irq12:TRAP_ENTRY_INTERRUPT(12)        /* IRQ Zilog serial chip         */
t_irq13:TRAP_ENTRY_INTERRUPT(13)        /* IRQ Audio Intr.               */
t_irq14:TRAP_ENTRY_INTERRUPT(14)        /* IRQ Timer #2                  */
t_nmi:	TRAP_ENTRY_INTERRUPT(15)
t_racc:	TRAP_ENTRY(def_trap)		/* General Register Access Error */
t_iacce:TRAP_ENTRY(def_trap)		/* Instr Access Error            */
t_bad22:TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
t_cpdis:TRAP_ENTRY(def_trap)		/* Co-Processor Disabled         */
t_uflsh:TRAP_ENTRY(def_trap)		/* Unimplemented FLUSH inst.     */
t_bad26:TRAP_ENTRY(def_trap)
	TRAP_ENTRY(def_trap)
t_cpexc:TRAP_ENTRY(def_trap)		/* Co-Processor Exception        */
t_dacce:TRAP_ENTRY(def_trap)		/* Data Access Error             */
t_hwdz:	TRAP_ENTRY(def_trap)		/* Division by zero, you lose... */
t_dserr:TRAP_ENTRY(def_trap)		/* Data Store Error              */
t_daccm:TRAP_ENTRY(def_trap)		/* Data Access MMU-Miss          */
	
ENTRY(start)
	/* Turn on Supervisor, EnableFloating, and all the PIL bits.
	 * Also puts us in register window zero with traps off.
	 */
	set	(PSR_PS | PSR_S | PSR_PIL | PSR_EF), %g2
	wr	%g2, 0x0, %psr
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
	or	%g3, 0x01, %g3
	set	vm_enabled, %g1
	jmpl	%g1, %g0
	 sta	%g3, [%g0] ASI_MMUREGS

vm_enabled:	
	/* Make window 0 valid, all other invalid */
	orn	%g0, 0x01, %g1
	wr	%g1, 0x0, %wim
	WRITE_PAUSE
	
	/* kernel stack */
	set	kstack_top, %g1
	or	%g0, %g1, %sp
	mov	0, %fp

	/* Start using the trap table  */
	set	trapbase, %g3
	wr	%g3, 0x0, %tbr

	/* Turn on traps so that we can call c-code. */	
	rd	%psr, %g3
	wr	%g3, PSR_ET, %psr

	call	init
	 nop

def_trap:
	set	kstack_top, %l5
	sub	%l5, TRAPFRAME_SZ, %l5

	STORE_GLOBALS(l5 + TF_G0)
	STORE_TRAPFRAME_OTHER(l5, l0, l1, l2, l4)
	or	%l5, %g0, %g7

	restore
	STORE_OUTS(g7 + TF_O0)
	STORE_LOCALS(g7 + TF_L0)
	STORE_INS(g7 + TF_I0)

	/* Have a saved trapframe, so can use any registers */
	rd	%psr, %l0
	wr	%l0, PSR_ET, %psr
	WRITE_PAUSE

	mov	%g0, %fp
	mov	%g7, %sp
	call	trap_handler
	 mov	%sp, %o0

window_spill:	
	mov	%l1, %g5
	mov	%l2, %g6

	/* Shift WIM bits right */
	sll	%l3, NWINDOWS - 1, %l4
	srl	%l3, 1, %l3
	or	%l3, %l4, %g7

	orcc	%l0, PSR_PS, %g0
	be	user_spill
	 nop

kernel_spill:
	restore
	mov	%g7, %wim
	WRITE_PAUSE

	STORE_WINDOW(sp)

	rd	%psr, %g7
	or	%g7, PSR_ET, %g7
	wr	%g7, %psr
	WRITE_PAUSE

	jmp	%g5
	 jmp	%g6	
	  nop

user_spill:
	restore
	mov	%g7, %wim
	WRITE_PAUSE

	rd	%psr, %g7
	or	%g7, PSR_ET, %g7
	andn	%g7, PSR_S, %g7
	wr	%g7, %psr
	WRITE_PAUSE

	/* XXX jmpl to a user provided handler */
	set	0x4000000, %g7
	jmpl	%g7, %g0
	 nop
	
window_fill:	
	mov	%l1, %g5
	mov	%l2, %g6

	/* Shift WIM bits left */
	sll	%l3, 1, %l4
	srl	%l3, NWINDOWS - 1, %l3
	or	%l3, %l4, %g7
	
	mov	%g0, %wim
	WRITE_PAUSE

	restore
	restore
	LOAD_WINDOW(sp)
	save
	save
	
	RETT_PSR(l0, l5)
	wr	%l0, %psr
	WRITE_PAUSE

	restore
	mov	%g7, %wim
	WRITE_PAUSE	
		
	jmp	%g5
	 jmp	%g6	
	  nop

trap_return:
	jmpl	%r17, %r0	/* old PC */
	rett	%r18		/* old nPC */
