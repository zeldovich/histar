#ifndef JOS_MACHINE_ARM
#define JOS_MACHINE_ARM

#define ARM_INST_ATTR \
	static __inline __attribute__((always_inline, no_instrument_function))

#define CPSR_MODE_MASK	0x1f			// cpu mode (priv & bank ctrl)
#define CPSR_MODE_USR	0x10			// user mode
#define CPSR_MODE_FIQ	0x11			// fiq mode
#define CPSR_MODE_IRQ	0x12			// irq mode
#define CPSR_MODE_SVC	0x13			// supervisor mode
#define CPSR_MODE_ABT	0x17			// abort mode
#define CPSR_MODE_UND	0x1b			// undefined instr mode
#define CPSR_MODE_SYS	0x1f			// system mode
#define CPSR_MODE(_x)	((_x) & CPSR_MODE_MASK)

#define CPSR_FIQ_OFF	0x00000040		// 1 => disable fast IRQs
#define CPSR_IRQ_OFF	0x00000080		// 1 => disable IRQs
#define CPSR_IMPABR_OFF	0x00000100		// 1 => disable imprecise aborts
#define CPSR_BE		0x00000200		// 1 => big-endian loads/stores

#define CPSR_ISET_MASK  0x01000020		// current instruction set
#define CPSR_ISET_ARM	0			// regular arm (word instrs)
#define CPSR_ISET_THUMB	0x00000020		// thumb (halfword instrs)
#define CPSR_ISET_JAZ	0x01000000		// jazelle tomfoolery
#define CPSR_ISET_RSVD	0x01000020		// reserved

#ifndef __ASSEMBLER__

//dirty bit emulation
int arm_dirtyemu(struct Pagemap *, const void *);

ARM_INST_ATTR uint32_t	cpsr_get(void);
ARM_INST_ATTR void	cpsr_set(uint32_t);
ARM_INST_ATTR uint32_t	cp15_ctrl_get(void);
ARM_INST_ATTR void	cp15_ctrl_set(uint32_t);
ARM_INST_ATTR uint32_t	cp15_cop_acc_ctrl_get(void);
ARM_INST_ATTR void	cp15_cop_acc_ctrl_set(uint32_t);
ARM_INST_ATTR uint32_t	cp15_fsr_get(void);
ARM_INST_ATTR void	cp15_fsr_set(uint32_t);	
ARM_INST_ATTR uint32_t	cp15_far_get(void);	
ARM_INST_ATTR void	cp15_far_set(uint32_t);	
ARM_INST_ATTR void	cp15_tlb_flush(void);	
ARM_INST_ATTR uint32_t	cp15_ttbr_get(void);
ARM_INST_ATTR void	cp15_ttbr_set(physaddr_t);

static uint32_t
cpsr_get()
{
	uint32_t val;
	__asm__ __volatile__("mrs %0, cpsr" : "=r" (val));
	return (val);
}

static void
cpsr_set(uint32_t val)
{
	__asm__ __volatile__("msr cpsr, %0" : : "r" (val));
}

static uint32_t
cp15_ctrl_get()
{
	uint32_t val;
	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r" (val));
	return (val);
}

static void
cp15_ctrl_set(uint32_t val)
{
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 0" : : "r" (val));
}

static uint32_t
cp15_cop_acc_ctrl_get()
{
	uint32_t val;
	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 2" : "=r" (val));
	return (val);
}

static void
cp15_cop_acc_ctrl_set(uint32_t val)
{
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 2" : : "r" (val));
}

static uint32_t
cp15_fsr_get()
{
	uint32_t val;
	__asm__ __volatile__("mrc p15, 0, %0, c5, c0, 0" : "=r" (val));
	return (val);
}

static void
cp15_fsr_set(uint32_t val)
{
	__asm__ __volatile__("mcr p15, 0, %0, c5, c0, 0" : : "r" (val));
}

static uint32_t
cp15_far_get()
{
	uint32_t val;
	__asm__ __volatile__("mrc p15, 0, %0, c6, c0, 0" : "=r" (val));
	return (val);
}

static void
cp15_far_set(uint32_t val)
{
	__asm__ __volatile__("mcr p15, 0, %0, c6, c0, 0" : : "r" (val));
}

static uint32_t
cp15_ttbr_get()
{
	uint32_t val;
	__asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r" (val));
	return (val);
}

static void
cp15_ttbr_set(physaddr_t base)
{
	__asm__ __volatile__("mcr p15, 0, %0, c2, c0, 0" : : "r" (base));
}

static void
cp15_tlb_flush()
{
	__asm__ __volatile__("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));
}

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_MACHINE_ARM */

