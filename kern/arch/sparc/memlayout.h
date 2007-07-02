#ifndef JOS_MACHINE_MEMLAYOUT_H
#define JOS_MACHINE_MEMLAYOUT_H

#include <machine/mmu.h>

/*
 * Virtual memory map
 *
 *   2^32 --------->  +---------------------------------------------+
 *                    |                                             |
 *                    |     1GB..2GB of physical address space      |
 *                    |                                             |
 *                    +---------------------------------------------+
 *                    |                                             |
 *                    |     0GB..1GB of physical address space      |
 *                    |                                             |
 *   PHYSBOT/ULIM ->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define PHYSBASE	0x80000000
#define KERNBASE	PHYSBASE

#define ULIM		PHYSBASE

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

/*
 * The linker script uses PHYSLOAD_ADDR and VIRTLOAD_ADDR.
 */
#define PHYSLOAD_ADDR   0x40000000
#define VIRTLOAD_ADDR   KERNBASE
#define LOAD_OFFSET     (VIRTLOAD_ADDR - PHYSLOAD_ADDR)

#define RELOC(x)        (x - LOAD_OFFSET)

#endif
