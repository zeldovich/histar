#include <machine/pmap.h>

/*
 * Boot page tables
 */

#define PTATTR __attribute__ ((aligned (4096), section (".data")))
#define KPT_BITS ((PT_ET_PTE << PTE_ET_SHIFT) | \
		  (PTE_ACC_SUPER << PTE_ACC_SHIFT) | PTE_C )

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

#define TRANS16MEG(n) (KPT_BITS | (((0x1000000UL * (n)) >> PGSHIFT) << PTE_PPN_SHIFT)),

/*
 * Map 1GB..3GB at PHYSBASE (0x80000000).
 */
struct Pagemap bootpt PTATTR = {
  .pm1_ent = {
    [128] = DO_64(64, TRANS16MEG)
    [192] = DO_64(128, TRANS16MEG)
  }
};

/*
 * Context table, inited during boot
 */
#define CTATTR __attribute__ ((aligned (4096), section (".data")))
struct Contexttable bootct CTATTR;

struct Trapcode idt[0x100];

