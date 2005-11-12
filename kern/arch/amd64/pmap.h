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

#define PHYSBASE UINT64 (0xffff800000000)
#define KSTACKTOP (KERNBASE - PGSIZE)
#define ULIM UINT64 (0x0000800000000000)

// At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
// IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
// at physical address EXTPHYSMEM.
#define IOPHYSMEM	0x0A0000
#define EXTPHYSMEM	0x100000

#define GD_UT 0x0b		/* User text */
#define GD_KT 0x10		/* Kernel text */
#define GD_D 0x18		/* Data (only needed for compatibility mode) */
#define GD_TSS 0x20		/* Task segment selector */
#define GD_UT32 0x30		/* user text for compatibility mode */

/* bootdata.c */
#ifndef __ASSEMBLER__
  extern uint64_t bootpts[];
extern uint64_t bootpds[];
extern uint64_t bootpd1[];
extern uint64_t bootpd2[];
extern uint64_t bootpdplo[];
extern uint64_t bootpdphi[];
extern uint64_t bootpml4[];

extern struct Tss tss;
extern uint64_t gdt[];
extern struct Pseudodesc gdtdesc;
extern struct Gatedesc idt[0x100];
extern struct Pseudodesc idtdesc;

#define MAXBUDDYORDER	10
#define BADBUDDYORDER	(MAXBUDDYORDER + 1)

struct klabel;
LIST_HEAD (Page_list, Page);
struct Page
{
  LIST_ENTRY (Page) pp_link;	/* free list link */
  struct klabel *pp_label;

  // Ref is the count of pointers (usually in page table entries)
  // to this page.  This only holds for pages allocated using 
  // page_alloc.  Pages allocated at boot time using pmap.c's
  // boot_alloc do not have valid reference count fields.
  uint16_t pp_ref;

  // What order free list is this page on if pp_ref == 0
  uint16_t pp_free_order;

#ifdef PAGE_USAGE
  char pp_name[PP_NAMESIZ];
  int marked;
#endif
};

extern struct Page *pages;
extern size_t npage;

void pmap_init (void);

inline int
get_order (size_t size)
{
  int order;

  size = (size - 1) >> (PGSHIFT - 1);
  order = -1;
  do {
    size >>= 1;
    order++;
  } while (size);
  return order;
}

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
    panic ("pa2page called with invalid pa %08x", pa);
  return &pages[PPN (pa)];
}

inline void *
page2kva (struct Page *pp)
{
  return (char *) PHYSBASE + page2pa (pp);
}

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_PMAP_H */
