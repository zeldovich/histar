#ifndef JOS_MACHINE_SPARC_TAG_H
#define JOS_MACHINE_SPARC_TAG_H

#include <machine/tag.h>

#define SPARC_TAG_INST_ATTR	\
	static __inline __attribute__((always_inline, no_instrument_function))

SPARC_TAG_INST_ATTR void wrtpcv(uint32_t pctag, uint32_t validbit);
SPARC_TAG_INST_ATTR void wrtdv(uint32_t dtag, uint32_t validbit);
SPARC_TAG_INST_ATTR void wrtperm(uint32_t pctag, uint32_t dtag, uint32_t permbits);
SPARC_TAG_INST_ATTR uint32_t read_dtag(const void *addr);
SPARC_TAG_INST_ATTR void write_dtag(const void *addr, uint32_t tag);
SPARC_TAG_INST_ATTR uint32_t read_pctag(void);
SPARC_TAG_INST_ATTR void write_pctag(uint32_t tag);

void
wrtpcv(uint32_t pctag, uint32_t validbit)
{
    __asm __volatile("wrtpcv %0, [%1]" : : "r" (validbit), "r" (pctag));
}

void
wrtdv(uint32_t dtag, uint32_t validbit)
{
    __asm __volatile("wrtdv %0, [%1]" : : "r" (validbit), "r" (dtag));
}

void
wrtperm(uint32_t pctag, uint32_t dtag, uint32_t permbits)
{
    uint32_t mergetag = (pctag << TAG_DATA_BITS) | dtag;
    __asm __volatile("wrtperm %0, [%1]" : : "r" (permbits), "r" (mergetag));
}

uint32_t
read_dtag(const void *addr)
{
    uint32_t tag;
    __asm __volatile("rdt [%1], %0" : "=r" (tag) : "r" (addr));
    return tag;
}

void
write_dtag(const void *addr, uint32_t tag)
{
    __asm __volatile("wrt %0, [%1]" : : "r" (tag), "r" (addr));
}

uint32_t
read_pctag(void)
{
    uint32_t tag;
    __asm __volatile("rdt %%pc, %0" : "=r" (tag));
    return tag;
}

void
write_pctag(uint32_t tag)
{
    __asm __volatile("wrt %0, %%pc" : : "r" (tag));
}

/*
 * Tag register read/write access functions.
 */

#define TAG_REG_RW(regname)				\
SPARC_TAG_INST_ATTR uint32_t read_##regname(void);	\
SPARC_TAG_INST_ATTR void write_##regname(uint32_t v);	\
							\
uint32_t						\
read_##regname(void)					\
{							\
    uint32_t v;						\
    __asm __volatile("rdtr %%" #regname ", %0"		\
		     : "=r" (v));			\
    return v;						\
}							\
							\
void							\
write_##regname(uint32_t v)				\
{							\
    __asm __volatile("wrtr %0, %%" #regname		\
		     : : "r" (v));			\
}

TAG_REG_RW(tsr)
TAG_REG_RW(rma)
TAG_REG_RW(et)
TAG_REG_RW(eoa)
#undef TAG_REG_RW

#endif
