#ifndef JOS_MACHINE_TAG_H
#define JOS_MACHINE_TAG_H

#define TAG_DEBUG	0

#define TSR_PET		(1 << 26)	/* Previous Enable Traps (from PSR) */
#define TSR_T		(1 << 25)	/* Trust */
#define TSR_PT		(1 << 24)	/* Previous Trust */
#define TSR_PEO		(1 << 1)	/* Previous Exception Override */
#define TSR_EO		(1 << 0)	/* Exception Override */

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
#define DTAG_P_ALLOC	6		/* Page allocator */
#define DTAG_TYPE_KOBJ	7		/* Kernel objects */
#define DTAG_TYPE_SYNC	8		/* Sync slot */
#define DTAG_STACK_0	9		/* Base stack */
#define DTAG_STACK_EX	10		/* 4 extra stacks */
#define DTAG_STACK_EXL	13		/* Last */
#define DTAG_DYNAMIC	14		/* First dynamically-allocated */

#define PCTAG_INIT	0		/* While bootstrapping */
#define PCTAG_P_ALLOC	1		/* Page allocator */
#define PCTAG_DYNAMIC	2		/* First dynamically-allocated */

/*
 * Tag trap errors
 */

#define TAG_TERR_RANGE	1
#define TAG_TERR_ALIGN	2

/*
 * Monitor call codes
 */

#define MONCALL_PCALL		1	/* Protected domain call */
#define MONCALL_PRETURN		2	/* Protected domain return */
#define MONCALL_TAGSET		3	/* Change tags ala memset */
#define MONCALL_DTAGALLOC	4	/* Allocate a tag */
#define MONCALL_THREAD_SWITCH	5	/* Change to another thread */
#define MONCALL_KOBJ_ALLOC	6	/* Allocate a kobject */
#define MONCALL_CATEGORY_ALLOC	7	/* Allocate a category */
#define MONCALL_SET_LABEL	8	/* Change label */
#define MONCALL_SET_CLEAR	9	/* Change clearance */
#define MONCALL_GATE_ENTER	10	/* Invoke a gate */
#define MONCALL_THREAD_START	11	/* Start a thread */
#define MONCALL_KOBJ_FREE	12	/* Free a kobject */
#define MONCALL_KOBJ_GC		13	/* GC an object */
#define MONCALL_FLUSHPERM	14	/* Flush permissions cache */
#define MONCALL_LABEL_COMPARE	15	/* Compare labels & add to cache */
#define MONCALL_MAX		16

#ifndef __ASSEMBLER__
#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/label.h>

enum {
    tag_type_data,
    tag_type_pc
};

#define	__krw__		__attribute__((section(".data_krw")));

#define PCALL_DEPTH	4		/* Maximum nesting level */

#define PROT_FUNC_WRAP2(moncall_pcall, stackframe_sz,			\
		        pctag, dtag, type, name)			\
    __asm__(".text\n"							\
	    ".align 4\n"						\
	    ".globl " #name "\n"					\
	    ".type " #name ", %function\n"				\
	    #name ":\n"							\
	    "rdtr   %tsr, %g1\n"					\
	    "srl    %g1, 25, %g1\n"					\
	    "andcc  %g1, 1, %g1\n"					\
	    "bnz    __preal_" #name "\n"				\
	    " nop\n"							\
	    "save   %sp, -" #stackframe_sz ", %sp\n"			\
	    "set    " #moncall_pcall ", %l0\n"				\
	    "set    " #pctag ", %l1\n"					\
	    "set    " #dtag ", %l2\n"					\
	    "set    __preal_" #name ", %l3\n"				\
	    "set    moncall_dummy, %g1\n"				\
	    "st	    %g0, [%g1]\n"					\
	    "ret; restore\n");

#define PROT_FUNC_WRAP(...) PROT_FUNC_WRAP2(__VA_ARGS__)

#define PROT_FUNC(pctag, dtag, type, name, ...)				\
    PROT_FUNC_WRAP(MONCALL_PCALL, STACKFRAME_SZ,			\
		   pctag, dtag, type, name)				\
									\
    type name(__VA_ARGS__);						\
    type __preal_##name(__VA_ARGS__);					\
    type __preal_##name(__VA_ARGS__)

extern const struct Label dtag_label[DTAG_DYNAMIC];
extern uint32_t moncall_dummy;
extern uintptr_t cur_stack_base;
extern const struct Thread *cur_mon_thread;
extern const char *moncall_names[MONCALL_MAX];

void	 tag_init(void);
void	 tag_init_late(void);

void	 tag_trap_entry(void) __attribute__((noreturn));
void	 tag_trap(struct Trapframe *tf, uint32_t err, uint32_t v)
		__attribute__((noreturn));
void	 tag_trap_return(const struct Trapframe *tf)
		__attribute__((noreturn));

void	 tag_set(const void *addr, uint32_t dtag, size_t n);
uint32_t tag_alloc(const struct Label *l, int tag_type);

int64_t	 monitor_call(uint32_t op, ...);
void	 pcall_trampoline(void) __attribute__((noreturn));

void	 tag_setperm(uint32_t pctag, uint32_t dtag, uint32_t permbits);
uint32_t tag_getperm(uint32_t pctag, uint32_t dtag);

void	 tag_is_kobject(const void *ptr, uint8_t type);
void	 tag_is_syncslot(const void *ptr);
#endif

#endif
