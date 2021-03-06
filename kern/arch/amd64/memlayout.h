#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

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

/*
 * gcc requires that the code resides either in the bottom 2GB of the
 * virtual address space (-mcmodel=small, medium) or the top 2GB of
 * the address space (-mcmodel=kernel).  Unfortunately this means
 * that we need to duplicate the physical memory somewhere else if
 * we want to access more than 2GB of physical memory.
 */

#define KERNBASE	UINT64 (0xffffffff80000000)
#define RELOC(x)	(CAST64 (x) - KERNBASE)

#define PHYSBASE	UINT64 (0xffff800000000000)
#define KSTACKTOP(cpu)	((KERNBASE - PGSIZE) - ((cpu) * 3 * PGSIZE))

#define ULIM		UINT64 (0x0000800000000000)

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

// User-mode (below ULIM) address space layout conventions.
#define UMMAPBASE	UINT64 (0x0000000100000000)
#define USTACKTOP	UINT64 (0x0000400000000000)
#define UFDBASE		UINT64 (0x0000400100000000)
#define UHEAP		UINT64 (0x0000400200000000)
#define UHEAPTOP	UINT64 (0x0000400300000000)
#define USTARTENVRO	UINT64 (0x0000400300000000)
#define UTLSBASE	UINT64 (0x0000400400000000)
#define UTLSTOP		UINT64 (0x0000400500000000)
#define USEGMAPENTS	UINT64 (0x0000400600000000)
#define ULDSO		UINT64 (0x0000400700000000)

#define UTLS_DEFSIZE	PGSIZE

#endif
