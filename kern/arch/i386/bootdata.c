
#include <machine/pmap.h>

/*
 * Boot page tables
 */

#define PTATTR __attribute__ ((aligned (4096), section (".data")))
#define KPDE_BITS (PTE_P|PTE_W|PTE_G|PTE_PS)

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

char kstack[KSTACK_SIZE] __attribute__ ((aligned (4096), section (".data")));

/*
 * Map first 1GB identically at bottom of VM space (for booting).
 * Map 0GB..1GB at PHYSBOT (0x80000000).
 * Map 3GB..4GB at PHYSTOP (0xc0000000).
 */
struct Pagemap bootpd PTATTR = {
  .pm_ent = {
    [0] = DO_256(0, TRANS4MEG)
    [512] = DO_256(0, TRANS4MEG)
    [768] = DO_256(768, TRANS4MEG)
  }
};

/*
 * Boot segments
 */
struct Tss tss = {
  .tss_sp = { [0] = { (uintptr_t) &kstack[KSTACK_SIZE], GD_KD, 0 } },
  .tss_iomb = offsetof (struct Tss, tss_iopb),
};

uint64_t gdt[] = {
  [GD_KT  >> 3] = SEG32(SEG_X|SEG_R, 0, 0xffffffff, 0),
  [GD_KD  >> 3] = SEG32(SEG_W, 0, 0xffffffff, 0),
  [GD_TSS >> 3]	= 0, 0,
  [GD_UD  >> 3] = SEG32(SEG_W, 0, 0xffffffff, 3),
  [GD_TD  >> 3] = SEG32(SEG_W, USTARTENVRO, PGSIZE, 3),
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
