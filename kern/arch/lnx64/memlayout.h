#ifndef JOS_INC_MEMLAYOUT_H
#define JOS_INC_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map
 *
 *   2^64 --------->  +---------------------------------------------+
 *                    |                                             |
 *                    |   Kernel memory (all symbols point here)    |
 *                    |                                             |
 *   KERNBASE ----->  +---------------------------------------------+
 *   KSTACKTOP ---->  +---------------------------------------------+
 *                    |            Kernel stack (2 pages)           |
 *                    +---------------------------------------------+
 *                    |                      :                      |
 *                    |                      :                      |
 *                    |                      :                      |
 *                    +---------------------------------------------+
 *                    |                                             |
 *                    |     All of physical memory mapped here      |
 *                    |                                             |
 *   PHYSBASE ----->  +---------------------------------------------+
 *                    |                                             |
 *                    |           2^47 to (2^64 - 2^47)             |
 *                    |         invalid virtual addresses           |
 *                    |                                             |
 *   ULIM --------->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define KERNBASE	UINT64 (0xffffffff80000000)
#define RELOC(x)	(CAST64 (x) - KERNBASE)

#define PHYSBASE	UINT64 (0xffff800000000000)
#define KSTACKTOP	(KERNBASE - PGSIZE)

#define ULIM		0x91000000
#define UBASE		0x90000000

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// User-mode (below ULIM) address space layout conventions.
#define UMMAPBASE	UINT64 (0x0000000100000000)
#define USTACKTOP	UINT64 (0x0000400000000000)
#define UFDBASE		UINT64 (0x0000400010000000)
#define UHEAP		UINT64 (0x0000400020000000)
#define USTARTENVRO	UINT64 (0x0000400030000000)
#define UTLS		UINT64 (0x0000400040000000)
#define USEGMAPENTS	UINT64 (0x0000400050000000)

#endif
