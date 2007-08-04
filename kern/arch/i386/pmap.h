#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifdef JOS_KERNEL
#include <machine/mmu.h>
#include <machine/memlayout.h>
#ifndef __ASSEMBLER__
#include <kern/lib.h>
#include <machine/types.h>
#include <inc/intmacro.h>
#include <kern/arch/amd64/pmap-x86.h>
#endif /* !__ASSEMBLER__ */
#endif /* JOS_KERNEL */

#define GD_KT	    (0x08 | 0x00)	/* Kernel text */
#define GD_KD	    (0x10 | 0x00)	/* Kernel data */
#define GD_TSS	    (0x18 | 0x00)	/* Task segment selector */
#define GD_UD	    (0x20 | 0x03)	/* User data */
#define	GD_TD	    (0x28 | 0x03)	/* Thread-local data */
#define GD_UT_NMASK (0x30 | 0x03)	/* User text, traps not masked */
#define GD_UT_MASK  (0x38 | 0x03)	/* User text, traps masked */

#define KSTACK_SIZE (2 * PGSIZE)

/* bootdata.c */
#if !defined(__ASSEMBLER__) && defined(JOS_KERNEL)
extern struct Pagemap bootpd;
extern char kstack[];

extern struct Tss tss;
extern uint64_t gdt[];
extern struct Pseudodesc gdtdesc;
extern struct Gatedesc idt[0x100];
extern struct Pseudodesc idtdesc;

/* mtrr.c */
int  mtrr_set(physaddr_t base, uint64_t nbytes, uint32_t type)
    __attribute__ ((warn_unused_result));

/* pmap.c */
typedef uint32_t ptent_t;

struct Pagemap {
    ptent_t pm_ent[NPTENTRIES];
};

void page_init(uint64_t lower_kb, uint64_t upper_kb);
void pmap_set_current_arch(struct Pagemap *pm);

static __inline void *
pa2kva(physaddr_t pa)
{
    if (pa < 0x40000000U)
	return (void *) (pa + PHYSBOT);
    if (pa >= 0xc0000000U)
	return (void *) (pa);
    panic("cannot map physaddr %x\n", pa);
}

static __inline physaddr_t
kva2pa(void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= PHYSBOT && va < PHYSBOT + (global_npages << PGSHIFT))
	return va - PHYSBOT;
    if (va >= PHYSTOP)
	return va;
    panic("kva2pa called with invalid kva %p", kva);
}

#endif /* !__ASSEMBLER__ && JOS_KERNEL */

#endif /* !JOS_MACHINE_PMAP_H */
