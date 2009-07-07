#ifndef JOS_MACHINE_MEMLAYOUT_HTCDREAM_H
#define JOS_MACHINE_MEMLAYOUT_HTCDREAM_H

#include <machine/mmu.h>

/*
 * HTC Dream virtual memory map - physical memory starts at 256MB,
 * MSM devices live in the upper 1.5GB of address space and some other
 * non-MSM devices live just at 2.35GB.
 *
 *   2^32 --------->  +---------------------------------------------+  4GB
 *                    |                                             |
 *                    |        2.375GB..4GB of physical memory      |
 *                    |                                             |
 *   PHYSTOP  ----->  +---------------------------------------------+  2.375GB
 *                    |                                             |
 *                    |        256MB..640MB of physical memory      |
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
#define PHYSTOP		0x98000000			/* 2.375GB */
#define KERNBASE	PHYSBOT
#define RELOC(x)	(CASTPTR(x) - 0x70000000)	/* ram starts @ 256MB */

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
#if defined(JOS_KERNEL) && !defined(__ASSEMBLER__)

#include <kern/lib.h>

static inline void *
pa2kva(physaddr_t pa)
{
	if (pa >= 0x10000000 && pa < 0x28000000)
		return ((void *)(pa + 0x70000000));
	if (pa >= PHYSTOP)
		return ((void *)pa);
	panic("pa2kva called with invalid pa 0x%x", pa);
}

static inline physaddr_t
kva2pa(void *kva)
{
	physaddr_t va = (physaddr_t)kva;

	if (va >= PHYSBOT && va < (PHYSBOT + (global_npages << PGSHIFT)))
		return (va - 0x70000000);
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
