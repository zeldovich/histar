#ifndef JOS_MACHINE_SPARC_H
#define JOS_MACHINE_SPARC_H

#define SPARC_INST_ATTR	\
	static __inline __attribute__((always_inline, no_instrument_function))

SPARC_INST_ATTR uint32_t rd_sp(void);
SPARC_INST_ATTR uint32_t rd_psr(void);
SPARC_INST_ATTR uint32_t rd_asr17(void);
SPARC_INST_ATTR uint32_t rd_tbr(void);
SPARC_INST_ATTR void flush(void);
SPARC_INST_ATTR uint32_t lda(uint32_t addr, uint32_t asi);
SPARC_INST_ATTR void sta(uint32_t addr, uint32_t val, uint32_t asi);

uint32_t 
rd_sp(void)
{
    uint32_t retval;
    __asm__ __volatile__("mov %%sp, %0"
			 : "=r" (retval));
    return retval;
}

uint32_t 
rd_psr(void)
{
    uint32_t retval;
    __asm__ __volatile__("rd %%psr, %0"
			 : "=r" (retval));
    return retval;
}

uint32_t 
rd_asr17(void)
{
    uint32_t retval;
    __asm__ __volatile__("rd %%asr17, %0"
			 : "=r" (retval));
    return retval;
}

uint32_t
rd_tbr(void)
{
    uint32_t retval;
    __asm__ __volatile__("rd %%tbr, %0"
			 : "=r" (retval));
    return retval;
}

uint32_t
lda(uint32_t regaddr, uint32_t asi)
{
    uint32_t retval;

    switch (asi) {
#define ASI_CASE(asival)			    \
    case asival:				    \
	__asm__ __volatile__("lda [%1] %2, %0"	    \
			     : "=r" (retval)	    \
			     : "r" (regaddr),	    \
			       "i" (asival)	    \
			     : "memory");	    \
	break;

    ASI_CASE(ASI_MMUREGS);
    ASI_CASE(ASI_BYPASS);
#undef ASI_CASE

    default:
	panic("Unknown ASI value %d in lda\n", asi);
    }

    return retval;
}

void
sta(uint32_t addr, uint32_t val, uint32_t asi)
{
    switch (asi) {
#define ASI_CASE(asival)			    \
    case asival:				    \
	__asm__ __volatile__("sta %0, [%1] %2"	    \
			     :			    \
			     : "r" (val),	    \
			       "r" (addr),	    \
			       "i" (asival)	    \
			     : "memory");	    \
	break;

    ASI_CASE(ASI_MMUREGS);
    ASI_CASE(ASI_BYPASS);
    ASI_CASE(ASI_DFLUSH);
    ASI_CASE(ASI_MMUFLUSH);
#undef ASI_CASE

    default:
	panic("Unknown ASI value %d in sta\n", asi);
    }
}

void
flush(void) 
{
    __asm__ __volatile__("flush");
}

#endif
