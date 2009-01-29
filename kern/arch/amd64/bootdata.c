#include <machine/param.h>
#include <machine/pmap.h>

/*
 * Boot page tables
 */

#define PTATTR __attribute__ ((aligned (4096), section (".data")))
#define KPDEP_BITS (PTE_P|PTE_W)
#define KPDE_BITS (KPDEP_BITS|PTE_PS|PTE_G)
#define KPTE_BITS (KPDEP_BITS|PTE_G)

#define DO_2(_start, _macro)					\
  _macro (((_start) + 0)) _macro (((_start) + 1))

#define DO_4(_start, _macro)					\
  DO_2 ((_start) + 0, _macro) DO_2 ((_start) + 2, _macro)

#define DO_8(_start, _macro)					\
  DO_4 ((_start) + 0, _macro) DO_4 ((_start) + 4, _macro)

#define DO_16(_start, _macro)					\
  DO_8 ((_start) + 0, _macro) DO_8 ((_start) + 8, _macro)

#define DO_64(_start, _macro)					\
  DO_16 ((_start) + 0, _macro) DO_16 ((_start) + 16, _macro)	\
  DO_16 ((_start) + 32, _macro) DO_16 ((_start) + 48, _macro)

#define DO_512(_start, _macro)					\
  DO_64 ((_start) + 0, _macro) DO_64 ((_start) + 64, _macro)	\
  DO_64 ((_start) + 128, _macro) DO_64 ((_start) + 192, _macro)	\
  DO_64 ((_start) + 256, _macro) DO_64 ((_start) + 320, _macro)	\
  DO_64 ((_start) + 384, _macro) DO_64 ((_start) + 448, _macro)

#define TRANS2MEG(n) (0x200000UL * (n) | KPDE_BITS), 

#if JOS_NCPU == 1
#define DO_NCPU(_macro) _macro((0))
#elif JOS_NCPU == 2
#define DO_NCPU(_macro) DO_2(0, _macro)
#elif JOS_NCPU == 4
#define DO_NCPU(_macro) DO_4(0, _macro)
#elif JOS_NCPU == 16
#define DO_NCPU(_macro) DO_16(0, _macro)
#else
#error unknown JOS_NCPU
#endif    

/* Page directory bootpds mapping the kernel stack (one page under KERNBASE) */
static char kstack[JOS_NCPU][2 * PGSIZE] 
       __attribute__ ((aligned (4096), section (".data")));

#define KSTACK_MAPPING(cpu)						\
    [509 - ((cpu) * 3)] = RELOC (&kstack[cpu][0 * PGSIZE]) + KPTE_BITS, \
    [510 - ((cpu) * 3)]	= RELOC (&kstack[cpu][1 * PGSIZE]) + KPTE_BITS,

struct Pagemap bootpts PTATTR = {
  .pm_ent = {
      DO_NCPU(KSTACK_MAPPING)
  }
};

struct Pagemap bootpds PTATTR = {
  .pm_ent = {
    [511] = RELOC (&bootpts) + KPDEP_BITS, /* sic - KPDE_BITS has PS, G */
  }
};

/* bootpd1-8 map the first 8 GBs of physical memory */
struct Pagemap bootpd1 PTATTR = { .pm_ent = { DO_512 (0,    TRANS2MEG) } };
struct Pagemap bootpd2 PTATTR = { .pm_ent = { DO_512 (512,  TRANS2MEG) } };
struct Pagemap bootpd3 PTATTR = { .pm_ent = { DO_512 (1024, TRANS2MEG) } };
struct Pagemap bootpd4 PTATTR = { .pm_ent = { DO_512 (1536, TRANS2MEG) } };
struct Pagemap bootpd5 PTATTR = { .pm_ent = { DO_512 (2048, TRANS2MEG) } };
struct Pagemap bootpd6 PTATTR = { .pm_ent = { DO_512 (2048, TRANS2MEG) } };
struct Pagemap bootpd7 PTATTR = { .pm_ent = { DO_512 (2048, TRANS2MEG) } };
struct Pagemap bootpd8 PTATTR = { .pm_ent = { DO_512 (2048, TRANS2MEG) } };

/*
 * Map first 2GB identically at bottom of VM space (for booting).
 * Map first 2GB at KERNBASE (-2 GB), where the kernel will run.
 * Map first 4GB at PHYSBASE.
 * Map the kernel stack right under KERNBASE.
 */
struct Pagemap bootpdplo PTATTR = {
  .pm_ent = {
    RELOC (&bootpd1) + KPDEP_BITS,
    RELOC (&bootpd2) + KPDEP_BITS,
    RELOC (&bootpd3) + KPDEP_BITS,
    RELOC (&bootpd4) + KPDEP_BITS,
    RELOC (&bootpd5) + KPDEP_BITS,
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
#define TSS_INIT(cpu)					\
  { .tss_rsp = { KSTACKTOP(cpu), KERNBASE, KERNBASE },	\
    .tss_iomb = offsetof (struct Tss, tss_iopb),	\
  },
    
struct Tss tss[JOS_NCPU] = {
  DO_NCPU(TSS_INIT)
};

#define GDT_INIT(cpu)					\
  { [GD_KT  >> 3] = SEG64 (SEG_X|SEG_R, 0),		\
    [GD_TSS >> 3]	= 0, 0,				\
    [GD_UD  >> 3] = SEG64 (SEG_W, 3),			\
    [GD_UT_NMASK >> 3] = SEG64 (SEG_X|SEG_R, 3),	\
    [GD_UT_MASK  >> 3] = SEG64 (SEG_X|SEG_R, 3),	\
  },

uint64_t gdt[JOS_NCPU][7] = {
  DO_NCPU(GDT_INIT)
};

#define GDTDESC_INIT(cpu)				\
  { .pd_lim = sizeof (gdt[(cpu)]) - 1,			\
    .pd_base = RELOC (&gdt[(cpu)])			\
  },

struct Pseudodesc gdtdesc[JOS_NCPU] = {
  DO_NCPU(GDTDESC_INIT)
};

struct Gatedesc idt[0x100];

struct Pseudodesc idtdesc = {
  .pd_lim = sizeof (idt) - 1,
  .pd_base = CAST64 (&idt)
};
