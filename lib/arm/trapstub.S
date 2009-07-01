#include <machine/asm.h>
#include <machine/arm.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <lib/utrapasm.h>

ENTRY(utrap_entry_asm)
	mov	r0, sp			// utf on stack at sp
	bl	utrap_entry

ENTRY(utrap_ret)
	mov	r1, r0
	mov	r0, #0	//UT_NMASK
	bl	sys_masked_jump
