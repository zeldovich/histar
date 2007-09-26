#ifndef JOS_MACHINE_TAG_H
#define JOS_MACHINE_TAG_H

#define TSR_PET		(1 << 26)	/* Previous Enable Traps (from PSR) */
#define TSR_T		(1 << 25)	/* Trust */
#define TSR_PT		(1 << 24)	/* Previous Trust */

#define ET_OPCODE_SHIFT	0
#define ET_OPCODE_MASK	0x7fff
#define ET_CAUSE_SHIFT	15
#define ET_CAUSE_MASK	0x7
#define ET_TAG_SHIFT	25
#define ET_TAG_MASK	0x7f

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

#define DTAG_DEVICE	0		/* Device memory-mapped regs */
#define DTAG_NOACCESS	1		/* Monitor access only */
#define DTAG_KEXEC	2		/* Read and exec-only kernel text */
#define DTAG_KRO	3		/* Read-only kernel data */
#define DTAG_KRW	4		/* Read-write kernel stack */
#define DTAG_MONCALL	5		/* Monitor call */
#define DTAG_PTEST	6		/* Protected domain test */
#define DTAG_DYNAMIC	7		/* First dynamically-allocated */

#define PCTAG_NONE	0		/* Just to be safe, for now */
#define PCTAG_PTEST	1		/* Protected domain test */
#define PCTAG_DYNAMIC	2		/* First dynamically-allocated */

/*
 * Tag trap errors
 */

#define TAG_TERR_RANGE	1
#define TAG_TERR_ALIGN	2

/*
 * Monitor call codes
 */

#define MONCALL_PCALL	1		/* Protected domain call */
#define MONCALL_PRETURN	2		/* Protected domain return */

#ifndef __ASSEMBLER__
#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/label.h>

enum {
    tag_type_data,
    tag_type_pc
};

#define PCALL_DEPTH	4		/* Maximum nesting level */

extern const struct Label dtag_label[DTAG_DYNAMIC];
extern uint32_t moncall_dummy;

void	 tag_init(void);

void	 tag_trap_entry(void) __attribute__((noreturn));
void	 tag_trap(struct Trapframe *tf, uint32_t err, uint32_t v)
		__attribute__((noreturn));
void	 tag_trap_return(const struct Trapframe *tf)
		__attribute__((noreturn));

void	 tag_set(const void *addr, uint32_t dtag, size_t n);
uint32_t tag_alloc(const struct Label *l, int tag_type);

uint32_t tag_call(void *func, uint32_t arg);
void	 moncall_trampoline(void) __attribute__((noreturn));
#endif

#endif
