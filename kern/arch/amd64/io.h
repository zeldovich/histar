#ifndef JOS_MACHINE_IO_H
#define JOS_MACHINE_IO_H

#include <machine/trapcodes.h>
#include <inc/types.h>

enum {
	BUS_CBUS		= 0,	/* Corollary CBUS */
	BUS_CBUSII,			/* Corollary CBUS II */
	BUS_EISA,			/* Extended ISA */
	BUS_FUTURE,			/* IEEE Futurebus */
	BUS_INTERN,			/* Internal bus */
	BUS_ISA,			/* Industry Standard Architecture */
	BUS_MBI,			/* Multibus I */
	BUS_MBII,			/* Multibus II */
	BUS_MCA,			/* Micro Channel Architecture */
	BUS_MPI,			/* MPI */
	BUS_MPSA,			/* MPSA */
	BUS_NUBUS,			/* Apple Macintosh NuBus */
	BUS_PCI,			/* Peripheral Component Interconnect */
	BUS_PCMCIA,			/* PC Memory Card International Association */
	BUS_TC,				/* DEC TurboChannel */
	BUS_VL,				/* VESA Local bus */
	BUS_VME,			/* VMEbus */
	BUS_XPRESS,			/* Express System Bus */
};

typedef uint32_t tbdp_t;

// Type (BUS_*), Bus number, Device number, Pin number (PCI INT_A#, ...)
#define MKBUS(t,b,d,p)	(((t)<<24)|(((b)&0xFF)<<16)|(((d)&0x1F)<<11)|(((p)&0x07)<<8))
#define BUSPNO(tbdp)	(((tbdp)>>8)&0x07)
#define BUSDNO(tbdp)	(((tbdp)>>11)&0x1F)
#define BUSBNO(tbdp)	(((tbdp)>>16)&0xFF)
#define BUSTYPE(tbdp)	((tbdp)>>24)
#define BUSUNKNOWN	((tbdp_t)-1)

#endif
