#ifndef JOS_KERN_PSTATE_H
#define JOS_KERN_PSTATE_H

#include <machine/types.h>
#include <kern/kobj.h>

//
// A rather limited persistent-store implementation.
// Eventually free lists & object location map should
// be btrees, and we should have a redo log.
//

#define PSTATE_MAGIC	0x4A4F535053544154ULL
#define PSTATE_VERSION	2

struct pstate_mapent {
    kobject_id_t id;
    kobject_type_t type;
    uint64_t flags;
    uint64_t offset;	// if 0, means free entry
    uint64_t pages;
};

// Up to 4K on-disk pages in this free list
#define NUM_PH_PAGES		PGSIZE
struct pstate_free_list {
    char inuse[NUM_PH_PAGES];
};

#define NUM_PH_OBJECTS		100
struct pstate_map {
    struct pstate_mapent ent[NUM_PH_OBJECTS];
};

struct pstate_header {
    uint64_t ph_magic;
    uint64_t ph_version;
    uint64_t ph_handle_counter;

    struct pstate_map ph_map;
    struct pstate_free_list ph_free;
};

int  pstate_init();
void pstate_sync();

#endif
