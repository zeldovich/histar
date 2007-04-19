#ifndef JOS_MACHINE_X86_H
#define JOS_MACHINE_X86_H

#include <inc/types.h>
#include <machine/mmu.h>
#include <machine/x86-common.h>

X86_INST_ATTR void lcr0(uint64_t val);
X86_INST_ATTR uint64_t rcr0(void);
X86_INST_ATTR uint64_t rcr2(void);
X86_INST_ATTR void lcr3(uint64_t val);
X86_INST_ATTR uint64_t rcr3(void);
X86_INST_ATTR void lcr4(uint64_t val);
X86_INST_ATTR uint64_t rcr4(void);
X86_INST_ATTR uint64_t read_rflags(void);
X86_INST_ATTR void write_rflags(uint64_t eflags);
X86_INST_ATTR uint64_t read_rbp(void);
X86_INST_ATTR uint64_t read_rsp(void);
X86_INST_ATTR uint64_t read_tscp(uint32_t *auxp);

void
lcr0(uint64_t val)
{
	__asm __volatile("movq %0,%%cr0" : : "r" (val));
}

uint64_t
rcr0(void)
{
	uint64_t val;
	__asm __volatile("movq %%cr0,%0" : "=r" (val));
	return val;
}

uint64_t
rcr2(void)
{
	uint64_t val;
	__asm __volatile("movq %%cr2,%0" : "=r" (val));
	return val;
}

void
lcr3(uint64_t val)
{
	__asm __volatile("movq %0,%%cr3" : : "r" (val));
}

uint64_t
rcr3(void)
{
	uint64_t val;
	__asm __volatile("movq %%cr3,%0" : "=r" (val));
	return val;
}

void
lcr4(uint64_t val)
{
	__asm __volatile("movq %0,%%cr4" : : "r" (val));
}

uint64_t
rcr4(void)
{
	uint64_t cr4;
	__asm __volatile("movq %%cr4,%0" : "=r" (cr4));
	return cr4;
}

uint64_t
read_rflags(void)
{
        uint64_t rflags;
        __asm __volatile("pushfq; popq %0" : "=r" (rflags));
        return rflags;
}

void
write_rflags(uint64_t rflags)
{
        __asm __volatile("pushq %0; popfq" : : "r" (rflags));
}

uint64_t
read_rbp(void)
{
        uint64_t rbp;
        __asm __volatile("movq %%rbp,%0" : "=r" (rbp));
        return rbp;
}

uint64_t
read_rsp(void)
{
        uint64_t rsp;
        __asm __volatile("movq %%rsp,%0" : "=r" (rsp));
        return rsp;
}

uint64_t
read_tscp(uint32_t *auxp)
{
	uint32_t a, d, c;
	__asm __volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
	*auxp = c;
	return ((uint64_t) a) | (((uint64_t) d) << 32);
}

#endif /* !JOS_MACHINE_X86_H */
