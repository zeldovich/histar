#ifndef JOS_INC_PMAP_H
#define JOS_INC_PMAP_H

#include <machine/mmu.h>
#ifndef __ASSEMBLER__
#include <inc/queue.h>
#include <kern/lib.h>
#endif /* !__ASSEMBLER__ */

/*
 * Virtual memory map
 *
 *   2^64 --------->  +---------------------------------------------+
 *                    |                                             |
 *                    |   Kernel memory (all symbols point here)    |
 *                    |                                             |
 *   KERNBASE ----->  +---------------------------------------------+
 *   KSTACKTOP ---->  +---------------------------------------------+
 *                    |            Kernel stack (2 pages)           |
 *                    +---------------------------------------------+
 *                    |                      :                      |
 *                    |                      :                      |
 *                    |                      :                      |
 *                    +---------------------------------------------+
 *                    |                                             |
 *                    |     All of physical memory mapped here      |
 *                    |                                             |
 *   PHYSBASE ----->  +---------------------------------------------+
 *                    |                                             |
 *                    |           2^47 to (2^64 - 2^47)             |
 *                    |         invalid virtual addresses           |
 *                    |                                             |
 *   ULIM --------->  +---------------------------------------------+
 *                    |                 user stack                  |
 *                    |                 user data                   |
 *                    |                 user text                   |
 *   0 ------------>  +---------------------------------------------+
 */

#define KERNBASE UINT64 (0xffffffff80000000)
#define RELOC(x) (CAST64 (x) - KERNBASE)

#define PHYSBASE UINT64 (0xffff800000000000)
#define KSTACKTOP (KERNBASE - PGSIZE)
#define ULIM UINT64 (0x0000800000000000)

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

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
struct Page
{
  LIST_ENTRY (Page) pp_link;	/* free list link */

  // Ref is the count of pointers (usually in page table entries)
  // to this page.  This only holds for pages allocated using 
  // page_alloc.  Pages allocated at boot time using pmap.c's
  // boot_alloc do not have valid reference count fields.
  uint32_t pp_ref;
};

struct Pagemap
{
    uint64_t pm_ent[NPTENTRIES];
};

extern struct Page *pages;
extern size_t npage;

void pmap_init (void);

void page_free (struct Page *pp);
int  page_alloc (struct Page **pp_store);
void page_decref (struct Page *pp);
struct Page *page_lookup (struct Pagemap *pgmap, void *va);
void page_remove (struct Pagemap *pgmap, void *va);
int  page_insert (struct Pagemap *pgmap, struct Page *pp, void *va, uint64_t perm);

int  page_cow (struct Pagemap *pgmap, void *va);
void page_map_decref (struct Pagemap *pgmap);
void page_map_addref (struct Pagemap *pgmap);
int  page_map_clone (struct Pagemap *pgmap, struct Pagemap **pm_store, int cow_data);

inline ppn_t
page2ppn (struct Page *pp)
{
  return pp - pages;
}

inline physaddr_t
page2pa (struct Page *pp)
{
  return page2ppn (pp) << PGSHIFT;
}

inline struct Page *
pa2page (physaddr_t pa)
{
  if (PPN (pa) >= npage)
    panic ("pa2page called with invalid pa %lx", pa);
  return &pages[PPN (pa)];
}

inline void *
page2kva (struct Page *pp)
{
  return (char *) PHYSBASE + page2pa (pp);
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

/* This macro takes a user supplied address and turns it into
 * something that will cause a fault if it is a kernel address.
 * ULIM itself is guaranteed never to contain a valid page.
 */
#define TRUP(_p)						\
({								\
	register typeof((_p)) __m_p = (_p);			\
	(uintptr_t) __m_p > ULIM ? (typeof(_p)) ULIM : __m_p;	\
})

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
