#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifndef __ASSEMBLER__
#include <machine/mmu.h>
#include <machine/sparc-config.h>
#include <inc/queue.h>

typedef uint32_t ptent_t;

LIST_HEAD(Pagemap2fl, Pagemap2);

struct Pagemap {
    ptent_t pm1_ent[256];
    struct Pagemap2fl fl;
};

struct Pagemap2 {
    union {
	ptent_t pm2_ent[64];
	LIST_ENTRY(Pagemap2) pm2_link;
    };
};

void mmu_init(void);
void page_init(void);
void page_init2(void);

typedef uint32_t ctxptr_t;

struct Contexttable {
    ctxptr_t ct_ent[CTX_NCTX];
};

#endif /* __ASSEMBLER__ */

#define KSTACK_SIZE	(2 * PGSIZE)

#endif
