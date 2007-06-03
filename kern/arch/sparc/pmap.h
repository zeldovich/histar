#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#include <machine/types.h>
#include <machine/mmu.h>

#define KSTACK_SIZE	(2 * PGSIZE)

typedef uint32_t ptent_t;

struct Pagemap {
    ptent_t pm1_ent[256];
};

struct Pagemap2 {
    ptent_t pm2_ent[64];
};

#endif
