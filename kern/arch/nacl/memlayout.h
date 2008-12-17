#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#ifndef __ASSEMBLER__
#include <machine/mmu.h>
#endif

#define USCRATCH	0x07002000
#define USPRING		0x07001000
#define ULIM		0x07000000
#define USTACKTOP	0x07000000
#define UTLS_DEFSIZE	PGSIZE

#endif
