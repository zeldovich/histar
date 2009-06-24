#include <machine/asm.h>

// r0 is jos_jmp_buf
ENTRY(jos_setjmp)
	stmia   r0, {r4-r14}
	mov	r0, #0
#ifdef __thumb__
	bx	lr
#else
	mov	pc, lr
#endif

// r0 is jos_jmp_buf, r1 is return value
ENTRY(jos_longjmp)
	ldmia	r0, {r4-r14}
	mov	r0, r1
#ifdef __thumb__
	bx	lr
#else
	mov	pc, lr
#endif
