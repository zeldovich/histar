#ifndef JOS_MACHINE_ARM
#define JOS_MACHINE_ARM

#include <machine/cpu.h>

#define CPSR_MODE_MASK	0x1f			// cpu mode (priv & bank ctrl)
#define CPSR_MODE_USR	0x10			// user mode
#define CPSR_MODE_FIQ	0x11			// fiq mode
#define CPSR_MODE_IRQ	0x12			// irq mode
#define CPSR_MODE_SVC	0x13			// supervisor mode
#define CPSR_MODE_ABT	0x17			// abort mode
#define CPSR_MODE_UND	0x1b			// undefined instr mode
#define CPSR_MODE_SYS	0x1f			// system mode
#define CPSR_MODE(_x)	((_x) & CPSR_MODE_MASK)

#define CPSR_PRIV_MASK	0x06f0fddf		// mask of privileged/resvd bits

#define CPSR_FIQ_OFF	0x00000040		// 1 => disable fast IRQs
#define CPSR_IRQ_OFF	0x00000080		// 1 => disable IRQs
#define CPSR_IMPABR_OFF	0x00000100		// 1 => disable imprecise aborts
#define CPSR_BE		0x00000200		// 1 => big-endian loads/stores

#define CPSR_ISET_MASK  0x01000020		// current instruction set
#define CPSR_ISET_ARM	0			// regular arm (word instrs)
#define CPSR_ISET_THUMB	0x00000020		// thumb (halfword instrs)
#define CPSR_ISET_JAZ	0x01000000		// jazelle tomfoolery
#define CPSR_ISET_RSVD	0x01000020		// reserved

#if defined(JOS_KERNEL) && !defined(__ASSEMBLER__)

extern uint32_t	cpsr_get(void);
extern void	cpsr_set(uint32_t);
extern uint32_t	cp15_ctrl_get(void);
extern void	cp15_ctrl_set(uint32_t);
extern uint32_t	cp15_cop_acc_ctrl_get(void);
extern void	cp15_cop_acc_ctrl_set(uint32_t);
extern uint32_t	cp15_fsr_get(void);
extern void	cp15_fsr_set(uint32_t);	
extern uint32_t	cp15_far_get(void);	
extern void	cp15_far_set(uint32_t);	
extern void	cp15_tlb_flush(void);	
extern void	cp15_tlb_flush_entry_arm11(void *);
extern uint32_t	cp15_ttbr_get(void);
extern void	cp15_ttbr_set(physaddr_t);
extern void	cp15_write_buffer_drain(void);
extern void	cp15_icache_invalidate_arm11(void);
extern void	cp15_icache_invalidate(void);
extern void	cp15_dcache_flush_invalidate_range_arm11(void *, uint32_t);
extern void	cp15_dcache_flush_invalidate_arm11(void);
extern void	cp15_dcache_flush_invalidate_tci(void);
extern void	cp15_dcache_flush_invalidate(void);
extern uint32_t	cp15_main_id_get(void);
extern uint32_t	cp15_cache_type_get(void);
extern void	cp15_wait_for_interrupt_arm1136(void);

#endif /* !(defined(JOS_KERNEL) && !defined(__ASSEMBLER__)) */

#endif /* !JOS_MACHINE_ARM */

