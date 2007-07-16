#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifndef __ASSEMBLER__
#include <machine/types.h>
#include <machine/mmu.h>
#include <machine/sparc-config.h>

typedef uint32_t ptent_t;

struct Pagemap {
    ptent_t pm1_ent[256];
};

struct Pagemap2 {
    ptent_t pm2_ent[64];
};

extern struct Pagemap bootpt;

void page_init(void);

extern physaddr_t maxpa;
extern physaddr_t minpa;

typedef uint32_t ctxptr_t;

struct Contexttable {
    ctxptr_t ct_ent[CTX_NCTX];
};

extern struct Contexttable bootct;

#endif /* __ASSEMBLER__ */

#define KSTACK_SIZE	(2 * PGSIZE)

#endif
