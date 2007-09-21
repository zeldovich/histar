#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map
 *
 *   2^32 --------->  +---------------------------------------------+
 *                    |                                             |
 *                    |        3GB..4GB of physical memory          |
 *                    |                                             |
 *   PHYSTOP  ----->  +---------------------------------------------+
 *                    |                                             |
 *                    |        0GB..1GB of physical memory          |
 *                    |                                             |
 *   PHYSBOT/ULIM ->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define PHYSBOT		0x80000000
#define PHYSTOP		0xc0000000
#define KERNBASE	PHYSBOT
#define RELOC(x)	(CASTPTR(x) - PHYSBOT)

#define ULIM		PHYSBOT

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at PHYSBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// User-mode (below ULIM) address space layout conventions.
#define USTACKTOP	ULIM
#define UMMAPBASE	0x40000000
#define UHEAP		0x50000000
#define UHEAPTOP	0x60000000
#define USTARTENVRO	0x60001000
#define UTLSBASE	0x60002000
#define UTLSTOP		0x60fff000
#define UFDBASE		0x61000000
#define USEGMAPENTS	0x62000000

#define UTLS_DEFSIZE	PGSIZE

#endif
