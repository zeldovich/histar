#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifdef JOS_KERNEL
#include <machine/mmu.h>
#include <machine/memlayout.h>
#ifndef __ASSEMBLER__
#include <machine/types.h>
#include <inc/intmacro.h>
#endif /* !__ASSEMBLER__ */
#endif /* JOS_KERNEL */

#define GD_KT	    (0x08 | 0x00)	/* Kernel text */
#define GD_TSS	    (0x10 | 0x00)	/* Task segment selector */
#define GD_TSS2	    (0x18 | 0x00)	/* TSS is a 16-byte descriptor */
#define GD_UD	    (0x20 | 0x03)	/* User data segment for iretq */
#define GD_UT_NMASK (0x28 | 0x03)	/* User text, traps not masked */
#define GD_UT_MASK  (0x30 | 0x03)	/* User text, traps masked */

/* bootdata.c */
#if !defined(__ASSEMBLER__) && defined(JOS_KERNEL)
extern struct Pagemap bootpd;

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

#endif /* !__ASSEMBLER__ && JOS_KERNEL */

#endif /* !JOS_MACHINE_PMAP_H */
