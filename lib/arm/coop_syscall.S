#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <machine/memlayout.h>
#include <inc/syscall_asm.h>
#include <inc/cooperate.h>

#define SYSCALL_ARGS	UTLSTOP-PGSIZE

/*
 * The following code is copied into a special text segment and hence
 * must be PIC.
 */
ENTRY(cooperate_syscall)
	/*
	 * The address of (struct coop_syscall_args) is in r0.
	 * 
	 * `coop_syscall_args' contains 8 uint64_t pointers. The first
	 * is actually a pointer to the syscall number. The next seven
	 * pointers point to the actual system call argument uint64_t's.
	 */
	mov	r3, r0			// r3 <- coop_syscall_arg ptr
	
	ldr	r0, [r3], #4		// r0 <- syscall number
	ldr	r0, [r0]
	adr	r1, _syscall_args	// r1 <- syscall arg destination
	ldr	r1, [r1]

	add	r6, r3, #28		// copy 7 pointers' worth
1:
	ldr	r7, [r3], #4
	ldmia	r7, {r4-r5}
	stmia	r1, {r4-r5}
	add	r1, r1, #8
	cmp	r3, r6
	bne	1b

	/*
	 * Now that we've copied all the system call arguments,
	 * execute the actual system call.
	 */
	adr	r1, _syscall_args	// r1 <- arg ptr
	ldr	r1, [r1]
	swi	#0			// r0 already is syscall #

	adr	r2, _coop_retval	// r2 <- COOP_RETVAL
	ldr	r2, [r2]
	stmia	r2, {r0,r1}		// *COOP_RETVAL = ret

	adr	r2, _coop_status	// r2 <- COOP_STATUS
	ldr	r2, [r2]
	mov	r3, #1
	mov	r4, #0
	stmia	r2, {r3,r4}		// *COOP_STATUS = 1

	mov	r0, #SYS_sync_wakeup
	adr	r1, _syscall_args	// r1 <- arg ptr
	ldr	r1, [r1]
	adr	r2, _coop_status	// r3 <- COOP_STATUS
	ldr	r2, [r2]
	mov	r3, #0
	stmia	r1, {r2,r3}
	swi	#0

	mov	r0, #SYS_self_halt
	swi	#0

1:	b	1b

_syscall_args:
	.word SYSCALL_ARGS
_coop_status:
	.word COOP_STATUS
_coop_retval:
	.word COOP_RETVAL

ENTRY(cooperate_syscall_end)
