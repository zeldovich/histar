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
 *       PHYSBASE ->  +---------------------------------------------+
 *                    |                 AHB memory                  |
 *   AHBBASE/ULIM ->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define PHYSBASE	0x90000000
#define KERNBASE	PHYSBASE
#define AHBBASE         0x80000000

#define ULIM		0x80000000

// User-mode (below ULIM) address space layout conventions.
#define USTACKTOP	ULIM
#define UWINOVERFLOW    0x3FFFE000 /* keep syncrhonized with */
#define UWINUNDERFLOW   0x3FFFF000 /* lib/sparc/Makefrag */
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
#define PHYSLOAD_ADDR   KERN_LOADADDR
#define VIRTLOAD_ADDR   KERNBASE
#define LOAD_OFFSET     (VIRTLOAD_ADDR - PHYSLOAD_ADDR)

#ifndef __ASSEMBLER__
#define RELOC(x)        ((uint32_t)x - LOAD_OFFSET)
#else
#define RELOC(x)        (x - LOAD_OFFSET)
#endif

#endif
