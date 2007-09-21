#ifndef JOS_MACHINE_X86_COMMON_H
#define JOS_MACHINE_X86_COMMON_H

#include <inc/types.h>
#include <machine/mmu.h>

#define X86_INST_ATTR	static __inline __attribute__((always_inline, no_instrument_function))

X86_INST_ATTR void breakpoint(void);
X86_INST_ATTR uint8_t inb(uint16_t port);
X86_INST_ATTR void insb(uint16_t port, void *addr, int cnt);
X86_INST_ATTR uint16_t inw(uint16_t port);
X86_INST_ATTR void insw(uint16_t port, void *addr, int cnt);
X86_INST_ATTR uint32_t inl(uint16_t port);
X86_INST_ATTR void insl(uint16_t port, void *addr, int cnt);
X86_INST_ATTR void outb(uint16_t port, uint8_t data);
X86_INST_ATTR void outsb(uint16_t port, const void *addr, int cnt);
X86_INST_ATTR void outw(uint16_t port, uint16_t data);
X86_INST_ATTR void outsw(uint16_t port, const void *addr, int cnt);
X86_INST_ATTR void outsl(uint16_t port, const void *addr, int cnt);
X86_INST_ATTR void outl(uint16_t port, uint32_t data);
X86_INST_ATTR void invlpg(const void *addr);
X86_INST_ATTR void wbinvd(void);
X86_INST_ATTR void lidt(void *p);
X86_INST_ATTR void lgdt(void *p);
X86_INST_ATTR void lldt(uint16_t sel);
X86_INST_ATTR void ltr(uint16_t sel);
X86_INST_ATTR void halt(void);
X86_INST_ATTR void sti(void);
X86_INST_ATTR void cli(void);
X86_INST_ATTR void cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp,
			 uint32_t *ecxp, uint32_t *edxp);
X86_INST_ATTR uint64_t read_tsc(void);
X86_INST_ATTR void fxsave(struct Fpregs *f);
X86_INST_ATTR void fxrstor(const struct Fpregs *f);
X86_INST_ATTR uint64_t read_msr(uint32_t msr);
X86_INST_ATTR void write_msr(uint32_t msr, uint64_t val);

void
breakpoint(void)
{
	__asm __volatile("int3");
}

uint8_t
inb(uint16_t port)
{
	uint8_t data;
	__asm __volatile("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

void
insb(uint16_t port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsb"			:
			 "+D" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "memory", "cc");
}

uint16_t
inw(uint16_t port)
{
	uint16_t data;
	__asm __volatile("inw %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

void
insw(uint16_t port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsw"			:
			 "+D" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "memory", "cc");
}

uint32_t
inl(uint16_t port)
{
	uint32_t data;
	__asm __volatile("inl %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

void
insl(uint16_t port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsl"			:
			 "+D" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "memory", "cc");
}

void
outb(uint16_t port, uint8_t data)
{
	__asm __volatile("outb %0,%w1" : : "a" (data), "d" (port));
}

void
outsb(uint16_t port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsb"		:
			 "+S" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "cc");
}

void
outw(uint16_t port, uint16_t data)
{
	__asm __volatile("outw %0,%w1" : : "a" (data), "d" (port));
}

void
outsw(uint16_t port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsw"		:
			 "+S" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "cc");
}

void
outsl(uint16_t port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsl"		:
			 "+S" (addr), "+c" (cnt)		:
			 "d" (port)				:
			 "cc");
}

void
outl(uint16_t port, uint32_t data)
{
	__asm __volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

void 
invlpg(const void *addr)
{ 
	__asm __volatile("invlpg (%0)" : : "r" (addr) : "memory");
}

void
wbinvd(void)
{
	__asm __volatile("wbinvd");
}

void
lidt(void *p)
{
	__asm __volatile("lidt (%0)" : : "r" (p));
}

void
lgdt(void *p)
{
	__asm __volatile("lgdt (%0)" : : "r" (p));
}

void
lldt(uint16_t sel)
{
	__asm __volatile("lldt %0" : : "r" (sel));
}

void
ltr(uint16_t sel)
{
	__asm __volatile("ltr %0" : : "r" (sel));
}

void 
halt(void) 
{
        __asm __volatile("sti; hlt" ::: "memory"); //memory -- DMA?
}

void 
sti(void) 
{
        __asm __volatile("sti" ::: "cc");
}

void 
cli(void) 
{
        __asm __volatile("cli" ::: "cc");
}

void
cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp,
      uint32_t *ecxp, uint32_t *edxp)
{
	uint32_t eax, ebx, ecx, edx;
	__asm volatile("cpuid" 
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		: "a" (info));
	if (eaxp)
		*eaxp = eax;
	if (ebxp)
		*ebxp = ebx;
	if (ecxp)
		*ecxp = ecx;
	if (edxp)
		*edxp = edx;
}

uint64_t
read_tsc(void)
{
	uint32_t a, d;
	__asm __volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((uint64_t) a) | (((uint64_t) d) << 32);
}

void
fxsave(struct Fpregs *f)
{
    __asm __volatile("fxsave %0" : "=m" (*f) : : "memory");
}

void
fxrstor(const struct Fpregs *f)
{
    __asm __volatile("fxrstor %0" : : "m" (*f));
}

uint64_t
read_msr(uint32_t msr)
{
    uint32_t hi, lo;
    __asm __volatile("rdmsr" : "=d" (hi), "=a" (lo) : "c" (msr));
    return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

void
write_msr(uint32_t msr, uint64_t val)
{
    uint32_t lo = val & 0xffffffff;
    uint32_t hi = val >> 32;
    __asm __volatile("wrmsr" : : "c" (msr), "a" (lo), "d" (hi));
}

/* Read/write the extra segment registers */
#define READ_SEGMENT_REG(rs)					\
X86_INST_ATTR uint16_t read_##rs(void);				\
uint16_t							\
read_##rs(void)							\
{								\
	uint16_t val;						\
	__asm __volatile("movw %%" #rs ",%0" : "=r" (val));	\
	return val;						\
}

#define WRITE_SEGMENT_REG(rs)					\
X86_INST_ATTR void write_##rs(uint16_t v);			\
void								\
write_##rs(uint16_t v)						\
{								\
	__asm __volatile("movw %0,%%" #rs : : "r" (v));		\
}

#define RW_SEGMENT_REG(rs) READ_SEGMENT_REG(rs) WRITE_SEGMENT_REG(rs)

READ_SEGMENT_REG(cs)
READ_SEGMENT_REG(ss)
RW_SEGMENT_REG(ds)
RW_SEGMENT_REG(es)
RW_SEGMENT_REG(fs)
RW_SEGMENT_REG(gs)

#undef RW_SEGMENT_REG
#undef READ_SEGMENT_REG
#undef WRITE_SEGMENT_REG

#endif /* !JOS_MACHINE_X86_COMMON_H */
