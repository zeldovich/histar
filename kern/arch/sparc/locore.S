#include <machine/asm.h>
#include <machine/psr.h>

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
	ba	window_spill
	 nop

window_fill:	
	ba	window_fill
	 nop
			
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

