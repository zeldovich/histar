#ifndef JOS_INC_PMAP_H
#define JOS_INC_PMAP_H

#include <machine/mmu.h>
#include <machine/memlayout.h>
#ifndef __ASSEMBLER__
#include <inc/queue.h>
#include <kern/lib.h>
#endif /* !__ASSEMBLER__ */

#define GD_UT	0x0b		/* User text */
#define GD_KT	0x10		/* Kernel text */
#define GD_D	0x18		/* Data (only needed for compatibility mode) */
#define GD_TSS	0x20		/* Task segment selector */
#define GD_UT32	0x30		/* user text for compatibility mode */
#define GD_UD	0x3b		/* user data segment -- shouldn't need it, but iretq is unhappy? */

/* bootdata.c */
#ifndef __ASSEMBLER__
extern struct Pagemap bootpml4;

extern struct Tss tss;
extern uint64_t gdt[];
extern struct Pseudodesc gdtdesc;
extern struct Gatedesc idt[0x100];
extern struct Pseudodesc idtdesc;

LIST_HEAD (Page_list, Page);
struct Page {
    LIST_ENTRY (Page) pp_link;	/* free list link */
};

struct Pagemap {
    uint64_t pm_ent[NPTENTRIES];
};

extern size_t npage;

void pmap_init (void);

int  page_alloc (void **p);
void page_free (void *p);

int  page_map_alloc (struct Pagemap **pm_store);
void page_map_free (struct Pagemap *pgmap);

int  page_insert (struct Pagemap *pgmap, void *page, void *va, uint64_t perm);
void page_remove (struct Pagemap *pgmap, void *va);
void *page_lookup (struct Pagemap *pgmap, void *va);

inline void *
pa2kva (physaddr_t pa)
{
    return (void*) (pa + PHYSBASE);
}

inline physaddr_t
kva2pa (void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= KERNBASE && va < KERNBASE + (npage << PGSHIFT))
	return va - KERNBASE;
    if (va >= PHYSBASE && va < PHYSBASE + (npage << PGSHIFT))
	return va - PHYSBASE;
    panic("kva2pa called with invalid kva %p", kva);
}

/*
 * This macro takes a user supplied address and turns it into
 * something that will cause a fault if it is a kernel address.
 */
#define TRUP(p) ((__typeof__(p)) (((uintptr_t)(p)) & ~(1L<<63)))

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
