
#include <machine/pmap.h>

/*
 * Boot page tables
 */

#define PTATTR __attribute__ ((aligned (4096), section (".data")))
#define KPDEP_BITS (PTE_P|PTE_W)
#define KPDE_BITS (KPDEP_BITS|PTE_PS|PTE_G)
#define KPTE_BITS (KPDEP_BITS|PTE_G)

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

#define DO_512(_start, _macro)					\
  DO_64 ((_start) + 0, _macro) DO_64 ((_start) + 64, _macro)	\
  DO_64 ((_start) + 128, _macro) DO_64 ((_start) + 192, _macro)	\
  DO_64 ((_start) + 256, _macro) DO_64 ((_start) + 320, _macro)	\
  DO_64 ((_start) + 384, _macro) DO_64 ((_start) + 448, _macro)

#define TRANS2MEG(n) (0x200000UL * (n) | KPDE_BITS), 

/* Page directory bootpds mapping the kernel stack (one page under KERNBASE) */
char kstack[2 * PGSIZE] __attribute__ ((aligned (4096), section (".data")));
struct Pagemap bootpts PTATTR = {
  .pm_ent = {
    [509] = RELOC (&kstack[0 * PGSIZE]) + KPTE_BITS,
    [510] = RELOC (&kstack[1 * PGSIZE]) + KPTE_BITS,
  }
};
struct Pagemap bootpds PTATTR = {
  .pm_ent = {
    [511] = RELOC (&bootpts) + KPDEP_BITS, /* sic - KPDE_BITS has PS, G */
  }
};

/* bootpd1-2 map the first and second GBs of physical memory */
struct Pagemap bootpd1 PTATTR = {
  .pm_ent = {
    DO_512 (0, TRANS2MEG)
  }
};
struct Pagemap bootpd2 PTATTR = {
  .pm_ent = {
    DO_512 (512, TRANS2MEG)
  }
};

/*
 * Map first two GB identically at bottom of VM space (for booting).
 * Map first two GB at KERNBASE (-2 GB), where the kernel will run.
 * Map first two GB at PHYSBASE.
 * Map the kernel stack right under KERNBASE.
 */
struct Pagemap bootpdplo PTATTR = {
  .pm_ent = {
    RELOC (&bootpd1) + KPDEP_BITS,
    RELOC (&bootpd2) + KPDEP_BITS,
  }
};
struct Pagemap bootpdphi PTATTR = {
  .pm_ent = {
    [509] = RELOC (&bootpds) + KPDEP_BITS,
    RELOC (&bootpd1) + KPDEP_BITS,
    RELOC (&bootpd2) + KPDEP_BITS,
  }
};
struct Pagemap bootpml4 PTATTR = {
  .pm_ent = {
    RELOC (&bootpdplo) + KPDEP_BITS,
    [256] = RELOC (&bootpdplo) + KPDEP_BITS,
    [511] = RELOC (&bootpdphi) + KPDEP_BITS,
  }
};

/*
 * Boot segments
 */
struct Tss tss = {
  .tss_rsp = { KSTACKTOP, KERNBASE, KERNBASE },
  .tss_iomb = offsetof (struct Tss, tss_iopb),
};

uint64_t gdt[] = {
  [GD_KT >> 3]	    = SEG64 (SEG_X|SEG_R, 0),
  [GD_UT >> 3]	    = SEG64 (SEG_X|SEG_R, 3),
  [GD_TSS >> 3]	    = 0, 0,
  [GD_UD >> 3]	    = SEG64 (SEG_W, 3),
};

struct Pseudodesc gdtdesc = {
  .pd_lim = sizeof (gdt) - 1,
  .pd_base = RELOC (&gdt)
};

struct Gatedesc idt[0x100];

struct Pseudodesc idtdesc = {
  .pd_lim = sizeof (idt) - 1,
  .pd_base = CAST64 (&idt)
};
