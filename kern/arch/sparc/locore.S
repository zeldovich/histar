#include <machine/asm.h>
#include <machine/psr.h>
#include <machine/asi.h>
#include <machine/sparc-config.h>
#include <machine/memlayout.h>
#include <machine/pmap.h>
#include <machine/tag.h>

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

/*
 * Tag traps.
 */

ENTRY(tag_trap_entry)
	wrtr	%g7, %rma
	set	(monstack_top - TRAPFRAME_SZ), %g7

	STORE_REG_SET(g, g7 + TF_G0)
	STORE_REG_SET(o, g7 + TF_O0)
	STORE_REG_SET(l, g7 + TF_L0)
	STORE_REG_SET(i, g7 + TF_I0)

	/* Restore the original %g7 and %rma values */
	rdtr	%rma, %g1
	st	%g1, [%g7 + TF_G7]
	set	tag_trap_entry, %g1
	wrtr	%g1, %rma

	/* Clear the %wim, we'll be doing some window magic in a bit */
	rd	%wim, %g2
	mov	0, %wim

	/* Compute the ET bit for the PSR: bit 26 in TSR, bit 5 in PSR */
	rdtr	%tsr, %g4
	srl	%g4, 21, %g4
	and	%g4, PSR_ET, %g4

	/* Store the PSR, we'll change it to flush windows */
	rd	%psr, %g3	    /* ET disabled */
	or	%g3, %g4, %g3	    /* Bring in the previous ET bit */
	st	%g3, [%g7 + TF_PSR]

	/* Get the CWP out of the PSR, and bump it by one, to flush windows */
	add	%g3, 1, %g3
	and	%g3, (NWINDOWS - 1), %g3
	mov	1, %g1
	sll	%g1, %g3, %g3		/* g3 = CWP mask bit */

	/* Prepare for range checks in a bit */
	set	kstack, %g6
	set	(KSTACK_SIZE - STACKFRAME_SZ), %g5

flush_more:
	andcc	%g2, %g3, %g0
	bnz	flush_done
	 restore

	cmp	%sp, %g0
	be	flush_done
	 sub	%sp, %g6, %g4

	/* Check that the %sp is in the kstack range */
	cmp	%g4, %g5
	bgu	flush_out_of_range
	 andcc	%sp, 7, %g0

	/* Check alignment for %sp */
	bnz	flush_bad_align
	 srl	%g2, 1, %g4

	/* Shift the wim bits over */
	sll	%g2, NWINDOWS - 1, %g2
	or	%g4, %g2, %g2

	STORE_WINDOW(sp)

	ba	flush_more
	 and	%g2, ((1 << NWINDOWS) - 1), %g2

flush_out_of_range:
	ba	flush_error
	 mov	TAG_TERR_RANGE, %g1

flush_bad_align:
	ba	flush_error
	 mov	TAG_TERR_ALIGN, %g1

flush_error:
	mov	%sp, %g2
	set	PSR_S | PSR_ET | PSR_PIL, %g4
	wr	%g4, %psr
	mov	2, %wim
	WRITE_PAUSE
	mov	%g1, %o2
	ba	tag_call_up
	 mov	%g2, %o3

flush_done:
	/* Store the new %wim after flushing */
	st	%g2, [%g7 + TF_WIM]

	/* Set CWP to 0, enable traps, and disable interrupts */
	set	PSR_S | PSR_ET | PSR_PIL, %g2
	wr	%g2, %psr

	/* Make window 1 invalid */
	mov	2, %wim

	/* Save other special registers */
	rdtr	%epc, %g1
	st	%g1, [%g7 + TF_PC]
	rdtr	%enpc, %g1
	st	%g1, [%g7 + TF_NPC]
	rd	%y, %g1
	st	%g1, [%g7 + TF_Y]

	mov	0, %o2		/* arg2: error code */
tag_call_up:
	/* Set up stack and args, and call into C code */
	mov	%g0, %fp
	mov	%g7, %sp
	mov	%g7, %o0	/* arg0: Trapframe */
	rd	%tbr, %o1	/* arg1: tbr */

	call	tag_trap
	 sub	%sp, STACKFRAME_SZ, %sp

ENTRY(tag_trap_return)
	mov	%g0, %wim

	mov	%o0, %g6	/* arg0: Trapframe */
	mov	%o1, %tbr	/* arg1: tbr */

	ld	[%g6 + TF_PSR], %l0
	ld	[%g6 + TF_WIM], %l1
	ld	[%g6 + TF_Y],   %l2

	andn	%l0, PSR_ET, %l3	/* PSR without ET */
	and	%l0, PSR_ET, %l4	/* Just the ET bit */
	or	%l4, (TSR_T >> 21), %l4	/* Keep the trust bit */
	sll	%l4, 21, %l4		/* Shift for TSR */
	wrtr	%l4, %tsr

	mov	%l2, %y
	mov	%l1, %wim
	mov	%l3, %psr

	ld	[%g6 + TF_PC],  %g1
	wrtr	%g1, %epc
	ld	[%g6 + TF_NPC], %g1
	wrtr	%g1, %enpc

	LOAD_REG_SET(i, g6 + TF_I0)
	LOAD_REG_SET(l, g6 + TF_L0)
	LOAD_REG_SET(o, g6 + TF_O0)
	LOAD_REG_SET(g, g6 + TF_G0)

	/* tret jumps to (epc,enpc), clears TSR_T, loads TSR_PET into PSR_ET */
	tret
	 nop

