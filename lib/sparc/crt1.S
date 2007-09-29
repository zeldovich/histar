#include <machine/asm.h>
#include <machine/mmu.h>
	
ENTRY(_start)
	// Args already in %o's, and kernel allocated space for a
	// stackframe.
	mov	%o0, %l0
	mov	%o1, %l1
	mov	%o2, %l2

	call	setup_env
	 nop

	mov	%l0, %o0
	mov	%l1, %o1
	mov	%l2, %o2

	call	_init
	 nop

	mov	%l0, %o0
	mov	%l1, %o1
	mov	%l2, %o2
	set	main, %o3

	call	libmain
	 nop
