#ifndef JOS_MACHINE_SPARC_COMMON_H
#define JOS_MACHINE_SPARC_COMMON_H

#include <machine/asi.h>
#include <machine/sparc.h>

#define SPARC_INLINE_ATTR \
	static __inline __attribute__((always_inline, no_instrument_function))

SPARC_INLINE_ATTR uint32_t lda_mmuregs(uint32_t regaddr);
SPARC_INLINE_ATTR void sta_mmuflush(uint32_t addr);
SPARC_INLINE_ATTR void sta_dflush(void);
SPARC_INLINE_ATTR uint32_t lda_bypass(uint32_t paddr);
SPARC_INLINE_ATTR void sta_bypass(uint32_t paddr, uint32_t value);
SPARC_INLINE_ATTR void tlb_flush_all(void);

uint32_t
lda_mmuregs(uint32_t regaddr)
{
    return lda(regaddr, ASI_MMUREGS);
}

void
sta_mmuflush(uint32_t addr)
{
    /* the value is always ignored */
    sta(addr, 0, ASI_MMUFLUSH);
}

void
sta_dflush(void)
{
    sta(0, 0, ASI_DFLUSH);
}

/* do a physical address bypass load, i.e. for 0x80000000 */
uint32_t
lda_bypass(uint32_t paddr)
{
    return lda(paddr, ASI_BYPASS);
}

/* do a physical address bypass write, i.e. for 0x80000000 */
void 
sta_bypass(uint32_t paddr, uint32_t value)
{
    return sta(paddr, value, ASI_BYPASS);
}

void
tlb_flush_all(void)
{
    /* flush both icache and dcache */
    flush();
    sta_dflush();
    /* flush entire TLB (pg 249-250 SPARC v8 manual) */
    sta_mmuflush(0x400);
}

#endif
