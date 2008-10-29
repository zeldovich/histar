#ifndef JOS_MACHINE_MP_H
#define JOS_MACHINE_MP_H

#include <machine/types.h>
#include <machine/mpreg.h>
#include <machine/io.h>

void		mp_init(void);
struct mp_fptr* mp_search(void);
uint32_t	mp_intrenable(uint32_t irq, tbdp_t tbdp);
void		mp_intrdisable(uint32_t trapno);

struct cpu {
    uint8_t		apicid;
    volatile char	booted;
};

struct apic {
    uint32_t		type;
    uint32_t		apicno;

    volatile uint32_t*	addr;		/* register base address */
    uint32_t		paddr;
    int			flags;		/* PCMP_BP|PCMP_EN */

    uint32_t		mre;		/* I/O APIC: maximum redirection entry */
};

struct aintr {
    struct apic*	apic;
    struct aintr*	next;
    struct mp_intr	intr;
    int			vno;		/* vector number if one has been assigned */
};

struct bus {
    uint8_t		type;
    uint8_t		busno;
    uint8_t		po;
    uint8_t		el;
    
    struct aintr*	aintr;		/* interrupts tied to this bus */
    struct bus*		next;
};

extern struct cpu  cpus[];
extern uint32_t	   ncpu;
extern struct cpu* bcpu;

#endif
