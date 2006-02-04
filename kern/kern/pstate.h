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

struct pstate_header {
    uint64_t ph_magic;
    uint64_t ph_version;

    uint64_t ph_handle_counter;
    uint64_t ph_user_root_handle;

    struct freelist ph_free ;
    struct btree_default ph_iobjs ;
    struct btree_default ph_map ;
};

void pstate_init(void);
int  pstate_load(void);
void pstate_sync(void);

// suspends cur_thread, and wakes it up when it should try again
int  pstate_swapin(kobject_id_t id);

#endif
