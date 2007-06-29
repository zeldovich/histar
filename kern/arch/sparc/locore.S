#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/win.h>
	
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
	
ENTRY(start)
	/* Turn on Supervisor, EnableFloating, and all the PIL bits.
	 * Also puts us in register window zero with traps off.
	 */
	set	(PSR_PS | PSR_S | PSR_PIL | PSR_EF), %g2
	wr	%g2, 0x0, %psr

	/* Make window 1 invalid */
	mov	2, %g1
	wr	%g1, 0x0, %wim
	
	/* XXX kernel stack */

	/* Start using the trap table  */
	set	trapbase, %g3
	wr	%g3, 0x0, %tbr

	/* Turn on traps so that we can call c-code. */	
	rd	%psr, %g3
	wr	%g3, PSR_ET, %psr

	call	init
	 nop
			
def_trap:
	ba	def_trap
	 nop

window_spill:
	mov	%g1, %l7
	
	/* Shift WIM bit right */
	sll	%l3, NWINDOWS_KERN - 1, %l4
	srl	%l3, 1, %l3
	or	%l3, %l4, %g1

	/* The CWP is decremented on overflow.  We save the window
	 * that the window trap handler is going to use next, and
	 * allow the faulting save to use the window the trap handler
	 * is using now.
	 */
	save
	
	mov	%g1, %wim
	nop; nop; nop

	st	%l0, [%sp + 0]
	st	%l1, [%sp + 4]
	st	%l2, [%sp + 8]
	st	%l3, [%sp + 12]
	st	%l4, [%sp + 16]
	st	%l5, [%sp + 20]
	st	%l6, [%sp + 24]
	st	%l7, [%sp + 28]
	st	%i0, [%sp + 32]
	st	%i1, [%sp + 36]
	st	%i2, [%sp + 40]
	st	%i3, [%sp + 44]
	st	%i4, [%sp + 48]
	st	%i5, [%sp + 52]
	st	%i6, [%sp + 56]
	st	%i7, [%sp + 60] 

	restore

	mov	%l7, %g1

	jmp	%l1
	 rett	%l2
			
window_fill:	

	/* Shift WIM bit left */
	sll	%l3, 1, %l4
	srl	%l3, NWINDOWS_KERN - 1, %l3
	or	%l3, %l4, %l3

	mov	%l3, %wim
	nop; nop; nop

	/* The CWP is decremented on underflow, so need to restore
	 * twice to reach the correct window.
	 */
	restore				
	restore				
        
	ld   [%sp + 0], %l0
	ld   [%sp + 4], %l1
	ld   [%sp + 8], %l2
	ld   [%sp + 12], %l3
	ld   [%sp + 16], %l4
	ld   [%sp + 20], %l5
	ld   [%sp + 24], %l6
	ld   [%sp + 28], %l7
	ld   [%sp + 32], %i0
	ld   [%sp + 36], %i1
	ld   [%sp + 40], %i2
	ld   [%sp + 44], %i3
	ld   [%sp + 48], %i4
	ld   [%sp + 52], %i5
	ld   [%sp + 56], %i6
	ld   [%sp + 60], %i7

	save
	save

	jmp  %l1
	 rett  %l2
			
trap_return:
	jmpl	%r17, %r0	/* old PC */
	rett	%r18		/* old nPC */
	
/*
 * XXX
 * Dummy symbols that don't actually do the right thing
 */

ENTRY(jos_longjmp)
ENTRY(jos_setjmp)
ENTRY(disk_io)
ENTRY(ide_poke)

