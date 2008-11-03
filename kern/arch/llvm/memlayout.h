#ifndef JOS_INC_MEMLAYOUT_H
#define JOS_INC_MEMLAYOUT_H

#include <machine/mmu.h>

#define KERNBASE	UINT64 (0xffffffff80000000)
#define RELOC(x)	(CAST64 (x) - KERNBASE)
#define ULIM		UINT64 (0xffffffffffffffff)
#define UFAKE		UINT64 (0x0000deadbeef0000)

// User-mode address space layout conventions.
#define USTACKTOP	UINT64 (0x0000400000000000)
#define UTLS_DEFSIZE	PGSIZE

#endif
