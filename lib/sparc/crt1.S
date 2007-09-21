#include <machine/asm.h>
#include <machine/mmu.h>
	
ENTRY(_start)
	// Args already in %o's, and kernel allocated space for a
	// stackframe

	call	setup_env
	 nop

	call	_init
	 nop

	call	libmain
	 nop
