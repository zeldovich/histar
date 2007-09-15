#ifndef JOS_MACHINE_TAG_H
#define JOS_MACHINE_TAG_H

#define TSR_EG		(1 << 27)	/* Exception Globals */
#define TSR_PEG		(1 << 26)	/* Previous Exception Globals */
#define TSR_T		(1 << 25)	/* Trust */
#define TSR_PT		(1 << 24)	/* Previous Trust */

#define ET_OPCODE_SHIFT	0
#define ET_OPCODE_MASK	0x7fff
#define ET_CAUSE_SHIFT	15
#define ET_CAUSE_MASK	0x7
#define ET_TAG_SHIFT	28
#define ET_TAG_MASK	0xf

#define ET_CAUSE_PCV	0
#define ET_CAUSE_DV	1
#define ET_CAUSE_READ	2
#define ET_CAUSE_WRITE	3
#define ET_CAUSE_EXEC	4

/*
 * Permissions table
 */

#define TAG_PERM_READ	(1 << 0)
#define TAG_PERM_WRITE	(1 << 1)
#define TAG_PERM_EXEC	(1 << 2)

#define TAG_PC_BITS	7
#define TAG_DATA_BITS	7

/*
 * Pre-defined tag values
 */

#define DTAG_NOACCESS	0		/* Monitor access only */
#define DTAG_KERNEL_RO	1		/* Read-only kernel text, data */
#define DTAG_DYNAMIC	2		/* Start dynamically-allocated */

#ifndef __ASSEMBLER__
#include <machine/types.h>
#include <machine/mmu.h>

void	tag_init(void);

void	tag_trap_entry(void) __attribute__((noreturn));
void	tag_trap(struct Trapframe *tf, uint32_t tbr) __attribute__((noreturn));
void	tag_trap_return(const struct Trapframe *tf,
			uint32_t tbr) __attribute__((noreturn));

void	tag_set(const void *addr, uint32_t dtag, size_t n);
#endif

#endif
