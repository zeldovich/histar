#ifndef JOS_INC_PMAP_H
#define JOS_INC_PMAP_H

#include <machine/mmu.h>
#include <machine/memlayout.h>
#include <machine/multiboot.h>
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

/* init.c */
extern char boot_cmdline[];

/* pmap.c */

struct Pagemap {
    uint64_t pm_ent[NPTENTRIES];
};

extern size_t global_npages;

extern struct page_stats {
    uint64_t pages_used;
    uint64_t pages_avail;
    uint64_t allocations;
    uint64_t failures;
} page_stats;

void pmap_init (struct multiboot_info *mbi);

int  page_alloc (void **p)
    __attribute__ ((warn_unused_result));
void page_free (void *p);

int  page_map_alloc (struct Pagemap **pm_store)
    __attribute__ ((warn_unused_result));
void page_map_free (struct Pagemap *pgmap);

int  page_insert (struct Pagemap *pgmap, void *page, void *va, uint64_t perm)
    __attribute__ ((warn_unused_result));
void *page_remove (struct Pagemap *pgmap, void *va);
void *page_lookup (struct Pagemap *pgmap, void *va);

static __inline void *
pa2kva (physaddr_t pa)
{
    return (void*) (pa + PHYSBASE);
}

static __inline physaddr_t
kva2pa (void *kva)
{
    physaddr_t va = (physaddr_t) kva;
    if (va >= KERNBASE && va < KERNBASE + (global_npages << PGSHIFT))
	return va - KERNBASE;
    if (va >= PHYSBASE && va < PHYSBASE + (global_npages << PGSHIFT))
	return va - PHYSBASE;
    panic("kva2pa called with invalid kva %p", kva);
}

static __inline ppn_t
pa2ppn (physaddr_t pa)
{
    ppn_t pn = pa >> PGSHIFT;
    if (pn > global_npages)
	panic("pa2ppn: pa 0x%lx out of range, npages %ld", pa, global_npages);
    return pn;
}

static __inline physaddr_t
ppn2pa (ppn_t pn)
{
    if (pn > global_npages)
	panic("ppn2pa: ppn %ld out of range, npages %ld", pn, global_npages);
    return (pn << PGSHIFT);
}

/*
 * Changes *ptrp such that it will not reference a kernel address,
 * and makes sure the address is paged in (might return -E_RESTART).
 */
int  page_user_incore(void **ptrp, uint64_t nbytes)
    __attribute__ ((warn_unused_result));

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
