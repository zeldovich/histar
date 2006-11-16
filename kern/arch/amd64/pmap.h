#ifndef JOS_INC_PMAP_H
#define JOS_INC_PMAP_H

#include <machine/mmu.h>
#include <machine/memlayout.h>
#ifndef __ASSEMBLER__
#include <kern/lib.h>
#include <kern/arch.h>
#include <inc/queue.h>
#endif /* !__ASSEMBLER__ */

#define GD_UT	0x0b		/* User text */
#define GD_KT	0x10		/* Kernel text */
#define GD_UD	0x1b		/* User data segment for iretq */
#define GD_TSS	0x20		/* Task segment selector */

/* bootdata.c */
#ifndef __ASSEMBLER__
extern struct Pagemap bootpml4;

extern struct Tss tss;
extern uint64_t gdt[];
extern struct Pseudodesc gdtdesc;
extern struct Gatedesc idt[0x100];
extern struct Pseudodesc idtdesc;

/* mtrr.c */
void mtrr_set(physaddr_t base, uint64_t nbytes, uint32_t type);

/* pmap.c */
struct Pagemap {
    uint64_t pm_ent[NPTENTRIES];
};

void pmap_init(uint64_t lower_kb, uint64_t upper_kb);

int  page_map_alloc(struct Pagemap **pm_store)
    __attribute__ ((warn_unused_result));
void page_map_free(struct Pagemap *pgmap);

typedef void (*page_map_traverse_cb)(const void *arg, uint64_t *ptep, void *va);
int  page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		       int create, page_map_traverse_cb cb, const void *arg)
    __attribute__ ((warn_unused_result));

int  pgdir_walk(struct Pagemap *pgmap, const void *va,
	       int create, uint64_t **pte_store)
    __attribute__ ((warn_unused_result));

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
 * Checks that [ptr .. ptr + nbytes) is valid user memory,
 * and makes sure the address is paged in (might return -E_RESTART).
 */
int  check_user_access(const void *ptr, uint64_t nbytes, uint32_t reqflags)
    __attribute__ ((warn_unused_result));

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
