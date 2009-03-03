#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map (same as i386)
 *
 *   2^32 --------->  +---------------------------------------------+  4GB
 *                    |                                             |
 *                    |        3GB..4GB of physical memory          |
 *                    |                                             |
 *   PHYSTOP  ----->  +---------------------------------------------+  3GB
 *                    |                                             |
 *                    |        0GB..1GB of physical memory          |
 *                    |                                             |
 *   PHYSBOT/ULIM ->  +---------------------------------------------+  2GB
 *                    |                                             |
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *                    |                                             |
 *                    |                                             |
 *   0 ------------>  +---------------------------------------------+
 *
 * NB: The kernel dynamically allocates stacks for the various exception
 *     modes.
 */

#ifndef __ASSEMBLER__
#define CASTPTR(x) ((uintptr_t) x)
#else
#define CASTPTR(x) (x)
#endif

#define PHYSBOT		0x80000000			/* 2GB */
#define PHYSTOP		0xc0000000			/* 3GB */
#define KERNBASE	PHYSBOT
#define RELOC(x)	(CASTPTR(x) - KERNBASE)

#define ULIM		PHYSBOT

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
#define ULDSO		0x63000000

#define UTLS_DEFSIZE	PGSIZE

#endif
