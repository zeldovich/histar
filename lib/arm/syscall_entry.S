#include <machine/asm.h>
#include <machine/trapcodes.h>

/*
 * from inc/syscall.h:
 *    uint64_t syscall(uint32_t num, uint64_t a1, uint64_t a2, uint64_t a3,
 *                     uint64_t a4,  uint64_t a5, uint64_t a6, uint64_t a7);
 *
 * The GNU EABI compiler appears to put the arguments as follows:
 *    r0        <-    num
 *    r2, r3    <-    a1
 *    [sp + 0]  <-    a2 
 *    [sp + 8]  <-    a3
 *    [sp + 16] <-    a4
 *       ...
 *    [sp + 40] <-    a7
 *
 * So, push a1 (r2, r3) and pass a pointer to the kernel.
 */

ENTRY(syscall)
	.cfi_startproc
	str	r3, [sp, #-4]!		// push a1
	str	r2, [sp, #-4]!		// keep pushing a1 
	mov	r1, sp			// pointer to args
	swi	#0			// fire! (r0 already contains syscall #)
	add	sp, sp, #8		// restore stack
#ifdef __thumb__
	bx	lr			// switch modes if needed and return
#else
	mov	pc, lr			// return
#endif
	.cfi_endproc
