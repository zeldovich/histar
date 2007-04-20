#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map
 *
 *   2^32 --------->  +---------------------------------------------+
 *   KSTACKTOP ---->  +---------------------------------------------+
 *                    |            Kernel stack (2 pages)           |
 *                    +---------------------------------------------+
 *                    |                      :                      |
 *                    |                      :                      |
 *                    |                      :                      |
 *                    +---------------------------------------------+
 *                    |                                             |
 *                    |   Kernel memory (all symbols point here)    |
 *                    |                                             |
 *   KERNBASE ----->  +---------------------------------------------+
 *                    |                                             |
 *                    |     All of physical memory mapped here      |
 *                    |                                             |
 *   PHYSBASE/ULIM->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define KSTACKTOP	(0xffffffff - PGSIZE + 1)
#define KERNBASE	0xc0000000
#define RELOC(x)	(CASTPTR(x) - KERNBASE)

#define PHYSBASE	0x80000000
#define ULIM		PHYSBASE

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
#define UTLSTOP		0x6ffff000
#define UFDBASE		0x61000000
#define USEGMAPENTS	0x62000000

#endif
