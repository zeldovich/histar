#ifndef JOS_KERN_PSTATE_H
#define JOS_KERN_PSTATE_H

#include <machine/types.h>
#include <kern/kobj.h>

#define PSTATE_MAGIC	0x4A4F535053544154ULL
#define PSTATE_VERSION	1

struct pstate_object_record {
    kobject_id_t id;
    uint64_t offset;
};

#define NUM_PH_OBJECTS		200

struct pstate_header {
    uint64_t ph_magic;
    uint64_t ph_version;

    uint64_t ph_handle_counter;

    struct pstate_object_record ph_map[NUM_PH_OBJECTS];
};

int  pstate_init();
void pstate_sync();

#endif
