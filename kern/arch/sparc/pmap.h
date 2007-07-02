#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <machine/sparc-config.h>

#define KSTACK_SIZE	(2 * PGSIZE)

typedef uint32_t ptent_t;

struct Pagemap {
    ptent_t pm1_ent[256];
};

struct Pagemap2 {
    ptent_t pm2_ent[64];
};

void page_init(void);

extern physaddr_t maxpa;
extern physaddr_t minpa;

typedef uint32_t ctxptr_t;

struct Contexttable {
    ctxptr_t ct_ent[CTX_NCTX];
};

#endif
