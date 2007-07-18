#ifndef JOS_MACHINE_SPARC_COMMON_H
#define JOS_MACHINE_SPARC_COMMON_H

#include <machine/asi.h>

#define SPARC_INST_ATTR	static __inline __attribute__((always_inline, no_instrument_function))

SPARC_INST_ATTR uint32_t rd_asr17(void);
SPARC_INST_ATTR uint32_t rd_tbr(void);
SPARC_INST_ATTR uint32_t lda_mmuregs(uint32_t regaddr);
SPARC_INST_ATTR void sta_mmuflush(uint32_t addr);
SPARC_INST_ATTR void sta_dflush(void);
SPARC_INST_ATTR void flush(void);
SPARC_INST_ATTR uint32_t lda_bypass(uint32_t paddr);
SPARC_INST_ATTR void sta_bypass(uint32_t paddr, uint32_t value);

uint32_t 
rd_asr17(void)
{
    uint32_t retval;
    __asm__ __volatile__("rd %%asr17, %0\n\t": "=r"(retval));
    return retval;
}

uint32_t
rd_tbr(void)
{
    uint32_t retval;
    __asm__ __volatile__("rd %%tbr, %0\n\t": "=r"(retval));
    return retval;
}

uint32_t
lda_mmuregs(uint32_t regaddr)
{
    uint32_t retval;
    __asm__ __volatile__("lda [%1] %2, %0\n\t":"=r"(retval):"r"(regaddr),
			 "i"(ASI_MMUREGS));
    return retval;
}

/* the value is always ignored */
void
sta_mmuflush(uint32_t addr)
{
    __asm__ __volatile__("sta %%g0, [%0] %1\n\t": :
			 "r" (addr),
			 "i" (ASI_MMUFLUSH) : "memory");
}

void
sta_dflush(void)
{
    __asm__ __volatile__("sta %%g0, [%%g0] %0\n\t": :
			 "i" (ASI_DFLUSH) : "memory");
}

void 
flush(void) 
{
    __asm__ __volatile__(" flush ");
}

/* do a physical address bypass load, i.e. for 0x80000000 */
uint32_t
lda_bypass(uint32_t paddr)
{
    uint32_t retval;
    __asm__ __volatile__("lda [%1] %2, %0\n\t":
			 "=r"(retval): "r"(paddr), "i"(ASI_BYPASS));
    return retval;
}

/* do a physical address bypass write, i.e. for 0x80000000 */
void 
sta_bypass(uint32_t paddr, uint32_t value)
{
    __asm__ __volatile__("sta %0, [%1] %2\n\t"::"r"(value), "r"(paddr),
			 "i"(ASI_BYPASS):"memory");
}

/* XXX */
#define LEON_BYPASS_LOAD_PA(x)		lda_bypass ((unsigned long)(x))
#define LEON_BYPASSCACHE_LOAD_VA(x)	leon_readnobuffer_reg ((unsigned long)(x))
#define LEON_BYPASS_STORE_PA(x,v)	sta_bypass((unsigned long)(x),(unsigned long)(v))
#define LEON_REGLOAD_PA(x)		leon_load_reg ((unsigned long)(x)+LEON_PREGS)
#define LEON_REGSTORE_PA(x,v)		leon_store_reg((unsigned long)(x)+LEON_PREGS,(unsigned long)(v))
#define LEON_REGSTORE_OR_PA(x,v)	LEON_REGSTORE_PA(x,LEON_REGLOAD_PA(x)|(unsigned long)(v))
#define LEON_REGSTORE_AND_PA(x,v)	LEON_REGSTORE_PA(x,LEON_REGLOAD_PA(x)&(unsigned long)(v))


#endif
