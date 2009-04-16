#ifndef JOS_MACHINE_MEMLAYOUT_GOLDFISH_H
#define JOS_MACHINE_MEMLAYOUT_GOLDFISH_H

#include <machine/mmu.h>

/*
 * Goldfish virtual memory map (same as i386) - physical memory starts
 * at 0 and devices live in the last 1GB.
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
#define RELOC(x)	(CASTPTR(x) - KERNBASE)		/* ram starts @ 0MB */
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

/*
 * Page addressing
 */
#ifndef __ASSEMBLER__

#include <kern/lib.h>

static inline void *
pa2kva(physaddr_t pa)
{
	if (pa < (1024*1024*1024))
		return ((void *)(pa + PHYSBOT));
	if (pa >= PHYSTOP)
		return ((void *)pa);
	panic("pa2kva called with invalid pa 0x%x", pa);
}

static inline physaddr_t
kva2pa(void *kva)
{
	physaddr_t va = (physaddr_t)kva;

	if (va >= PHYSBOT && va < (PHYSBOT + (global_npages << PGSHIFT)))
		return (va - PHYSBOT);
	if (va >= PHYSTOP)
		return (va);
	panic("kva2pa called with invalid kva %p", kva);
}

static inline ppn_t
pa2ppn(physaddr_t pa)
{
	ppn_t npage = pa >> PGSHIFT;
	if (npage > global_npages)
		panic("pa2ppn: 0x%x too far out", pa);
	return (npage);
}

static inline physaddr_t
ppn2pa(ppn_t pn)
{
	return (pn << PGSHIFT);
}
#endif

#endif
