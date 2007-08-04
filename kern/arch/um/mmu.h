#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#include <machine/types.h>
#include <machine/um.h>
#include <kern/lib.h>

#define PGSHIFT 12
#define PGSIZE (1 << PGSHIFT)
#define PGOFF(la) (((uintptr_t) (la)) & 0xFFF)
#define PTE_ADDR(pte) 0

struct Trapframe {
};

struct Trapframe_aux {
};

struct Fpregs {
};

static __inline ppn_t
pa2ppn(physaddr_t pa)
{
    return pa >> PGSHIFT;
}

static __inline physaddr_t
ppn2pa(ppn_t pn)
{
    return pn << PGSHIFT;
}

static __inline void *
pa2kva(physaddr_t pa)
{
    return um_mem_base + pa;
}

static __inline physaddr_t
kva2pa(void *kva)
{
    physaddr_t addr = (physaddr_t) kva;
    physaddr_t base = (physaddr_t) um_mem_base;
    assert(addr >= base && addr < base + um_mem_bytes);
    return addr - base;
}

#endif
