#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/mmu.h>
#include <machine/win.h>
		
	.text
	.align 4

/* Registers to not touch at all. */
#define t_psr        l0 /* Set by caller */
#define t_pc         l1 /* Set by caller */
#define t_npc        l2 /* Set by caller */
#define t_wim        l3 /* Set by caller */
#define t_twinmask   l4 /* Set at beginning of this entry routine. */
#define t_retpc      l5 /* If you change this, change macro in asm.h */
#define t_lscratch   l7 /* Scratch space */
#define t_kstack     g6 /* New stack pointer value */
#define t_gscratch   g7 /* Scratch space */
	.globl trap_setup
trap_setup:
	/* Calculate the mask of trap window */
	mov	1, %t_twinmask
	andcc	%t_psr, PSR_PS, %g0
	be	def_user_trap
	 sll	%t_twinmask, %t_psr, %t_twinmask

	/* Get some stack space for a trap frame */
	sub	%fp, TRAPFRAME_SZ, %t_kstack

	/* Store stuff that can be read from this window */
	STORE_TRAPFRAME_OTHER(t_kstack, t_psr, t_pc, t_npc, t_lscratch)
	
	andcc	%t_twinmask, %t_wim, %g0
	bne	trap_setup_kernel_spill
	 nop

	/* Get rest of trap frame */
	restore
	STORE_TRAPFRAME_REGFILE(t_kstack) 
	save	
	
	wr	%t_psr, PSR_ET, %psr
	WRITE_PAUSE

	jmpl	%t_retpc + 0x8, %g0
	 mov	%t_kstack, %sp	

trap_setup_kernel_spill:
	/* We will probably end up halting, but dump the register
	 * window just in case and shift the WIM bit right.
	 */
	sll	%t_wim, NWINDOWS_KERN - 1, %t_lscratch
	srl	%t_wim, 1, %t_gscratch
	or	%t_gscratch, %t_lscratch, %t_gscratch

	save

	mov	%t_gscratch, %wim
	WRITE_PAUSE

	STORE_WINDOW(sp)

	restore
	
	wr	%t_psr, PSR_ET, %psr
	WRITE_PAUSE

	/* Get rest of trap frame */
	restore
	STORE_TRAPFRAME_REGFILE(t_kstack) 
	save	
	
	jmpl	%t_retpc + 0x8, %g0
	 mov	%t_kstack, %sp
	
def_user_trap:
	ba	def_user_trap
	 nop
