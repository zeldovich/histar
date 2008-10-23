#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#include <machine/mmu.h>
#include <machine/memlayout.h>
#ifndef __ASSEMBLER__
#include <kern/lib.h>
#include <inc/queue.h>
#endif /* !__ASSEMBLER__ */

#define GD_KT	    (0x08 | 0x00)	/* Kernel text */
#define GD_TSS	    (0x10 | 0x00)	/* Task segment selector */
#define GD_TSS2	    (0x18 | 0x00)	/* TSS is a 16-byte descriptor */
#define GD_UD	    (0x20 | 0x03)	/* User data segment for iretq */
#define GD_UT_NMASK (0x28 | 0x03)	/* User text, traps not masked */
#define GD_UT_MASK  (0x30 | 0x03)	/* User text, traps masked */

/* bootdata.c */
#ifndef __ASSEMBLER__

/* pmap.c */
typedef uint64_t ptent_t;

struct Pagemapent {
    void *va;
    ptent_t pte;
};

#define NPME 2
struct Pagemap {
    struct Pagemapent pme[NPME];
};

void page_init(uint64_t lower_kb, uint64_t upper_kb);

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
