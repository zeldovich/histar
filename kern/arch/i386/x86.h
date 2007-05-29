#ifndef JOS_MACHINE_X86_H
#define JOS_MACHINE_X86_H

#include <inc/types.h>
#include <machine/mmu.h>
#include <kern/arch/amd64/x86-common.h>

X86_INST_ATTR void lcr0(uint32_t val);
X86_INST_ATTR uint32_t rcr0(void);
X86_INST_ATTR uint32_t rcr2(void);
X86_INST_ATTR void lcr3(uint32_t val);
X86_INST_ATTR uint32_t rcr3(void);
X86_INST_ATTR void lcr4(uint32_t val);
X86_INST_ATTR uint32_t rcr4(void);
X86_INST_ATTR uint32_t read_eflags(void);
X86_INST_ATTR void write_eflags(uint32_t eflags);
X86_INST_ATTR uint32_t read_esp(void);
X86_INST_ATTR uint32_t read_ebp(void);

void
lcr0(uint32_t val)
{
	__asm __volatile("movl %0,%%cr0" : : "r" (val));
}

uint32_t
rcr0(void)
{
	uint32_t val;
	__asm __volatile("movl %%cr0,%0" : "=r" (val));
	return val;
}

uint32_t
rcr2(void)
{
	uint32_t val;
	__asm __volatile("movl %%cr2,%0" : "=r" (val));
	return val;
}

void
lcr3(uint32_t val)
{
	__asm __volatile("movl %0,%%cr3" : : "r" (val));
}

uint32_t
rcr3(void)
{
	uint32_t val;
	__asm __volatile("movl %%cr3,%0" : "=r" (val));
	return val;
}

void
lcr4(uint32_t val)
{
	__asm __volatile("movl %0,%%cr4" : : "r" (val));
}

uint32_t
rcr4(void)
{
	uint32_t cr4;
	__asm __volatile("movl %%cr4,%0" : "=r" (cr4));
	return cr4;
}

uint32_t
read_eflags(void)
{
	uint32_t eflags;
	__asm __volatile("pushfl; popl %0" : "=r" (eflags));
	return eflags;
}

void
write_eflags(uint32_t eflags)
{
	__asm __volatile("pushl %0; popfl" : : "r" (eflags));
}

uint32_t
read_esp(void)
{
	uint32_t esp;
	__asm __volatile("movl %%esp,%0" : "=r" (esp));
	return esp;
}

uint32_t
read_ebp(void)
{
	uint32_t ebp;
	__asm __volatile("movl %%ebp,%0" : "=r" (ebp));
	return ebp;
}

#endif /* !JOS_MACHINE_X86_H */
