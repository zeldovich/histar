#ifndef JOS_KERN_PSTATE_H
#define JOS_KERN_PSTATE_H

#include <machine/types.h>
#include <kern/kobj.h>
#include <kern/freelist.h>

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

#define NUM_PH_OBJECTS		200
struct pstate_map {
    struct pstate_mapent ent[NUM_PH_OBJECTS];
};

struct pstate_header {
    uint64_t ph_magic;
    uint64_t ph_version;

    uint64_t ph_handle_counter;
    uint64_t ph_user_root_handle;

    struct pstate_map ph_map;
    struct freelist ph_free ;
};

int  pstate_init(void);
void pstate_reset(void);
void pstate_sync(void);

// suspends cur_thread, and wakes it up when it should try again
int  pstate_swapin(kobject_id_t id);

#endif
