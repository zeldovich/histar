
#include <machine/pmap.h>

/*
 * Boot page tables
 */

#define PTATTR __attribute__ ((aligned (4096), section (".data")))
#define KPT_COM_BITS (PTE_P|PTE_W)
#define KPDE_BITS (KPT_COM_BITS|PTE_G|PTE_PS)
#define KPTE_BITS (KPT_COM_BITS|PTE_G)

#define DO_8(_start, _macro)				\
  _macro (((_start) + 0)) _macro (((_start) + 1))	\
  _macro (((_start) + 2)) _macro (((_start) + 3))	\
  _macro (((_start) + 4)) _macro (((_start) + 5))	\
  _macro (((_start) + 6)) _macro (((_start) + 7))

#define DO_64(_start, _macro)					\
  DO_8 ((_start) + 0, _macro) DO_8 ((_start) + 8, _macro)	\
  DO_8 ((_start) + 16, _macro) DO_8 ((_start) + 24, _macro)	\
  DO_8 ((_start) + 32, _macro) DO_8 ((_start) + 40, _macro)	\
  DO_8 ((_start) + 48, _macro) DO_8 ((_start) + 56, _macro)

#define DO_256(_start, _macro)					\
  DO_64 ((_start) + 0, _macro) DO_64 ((_start) + 64, _macro)	\
  DO_64 ((_start) + 128, _macro) DO_64 ((_start) + 192, _macro)

#define TRANS4MEG(n) (0x400000UL * (n) | KPDE_BITS), 

/* Page table mapping the kernel stack (one page under 2^32) */
char kstack[2 * PGSIZE] __attribute__ ((aligned (4096), section (".data")));
struct Pagemap bootpts PTATTR = {
  .pm_ent = {
    [1021] = RELOC (&kstack[0 * PGSIZE]) + KPTE_BITS,
    [1022] = RELOC (&kstack[1 * PGSIZE]) + KPTE_BITS,
  }
};

/*
 * Map first 1GB identically at bottom of VM space (for booting).
 * Map first 512MB at KERNBASE (0xc0000000), where the kernel will run.
 * Map first 1GB at PHYSBASE (0x80000000).
 * Map the kernel stack a page under 2^32.
 */
struct Pagemap bootpd PTATTR = {
  .pm_ent = {
    [0] = DO_256(0, TRANS4MEG)
    [512] = DO_256(0, TRANS4MEG)
    [768] = DO_64(0, TRANS4MEG)
    [832] = DO_64(64, TRANS4MEG)
    [1023] = RELOC(&bootpts) + KPT_COM_BITS
  }
};

/*
 * Boot segments
 */
struct Tss tss = {
  .tss_sp = { [0] = { KSTACKTOP, GD_KT, 0 } },
  .tss_iomb = offsetof (struct Tss, tss_iopb),
};

uint64_t gdt[] = {
  [GD_KT  >> 3] = SEG32(SEG_X|SEG_R, 0, 0xffffffff, 0),
  [GD_TSS >> 3]	= 0, 0,
  [GD_UD  >> 3] = SEG32(SEG_W, 0, 0xffffffff, 3),
  [GD_UT_NMASK >> 3] = SEG32(SEG_X|SEG_R, 0, 0xffffffff, 3),
  [GD_UT_MASK  >> 3] = SEG32(SEG_X|SEG_R, 0, 0xffffffff, 3),
};

struct Pseudodesc gdtdesc = {
  .pd_lim = sizeof(gdt) - 1,
  .pd_base = RELOC(&gdt)
};

struct Gatedesc idt[0x100];

struct Pseudodesc idtdesc = {
  .pd_lim = sizeof(idt) - 1,
  .pd_base = (uintptr_t) &idt
};
