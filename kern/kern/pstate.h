#ifndef JOS_KERN_PSTATE_H
#define JOS_KERN_PSTATE_H

#include <machine/types.h>
#include <kern/kobj.h>
#include <kern/freelist.h>
#include <kern/handle.h>

//
// A rather limited persistent-store implementation.
// Eventually free lists & object location map should
// be btrees, and we should have a redo log.
//

#define PSTATE_MAGIC	0x4A4F535053544154ULL
#define PSTATE_VERSION	2

struct pstate_header {
	// needs to be in 1st sector of header
    char ph_applying ;
    
    uint64_t ph_magic;
    uint64_t ph_version;

    uint64_t ph_handle_counter;
    uint64_t ph_user_root_handle;
    uint64_t ph_user_msec;
    uint8_t ph_handle_key[HANDLE_KEY_SIZE];

    uint8_t ph_free[sizeof(struct freelist)] ;
    uint8_t ph_iobjs[sizeof(struct btree_default)] ;
    uint8_t ph_map[sizeof(struct btree_default)] ;
};

void pstate_init(void);
int  pstate_load(void) __attribute__ ((warn_unused_result));
void pstate_sync(void);

// suspends cur_thread, and wakes it up when it should try again
int  pstate_swapin(kobject_id_t id) __attribute__ ((warn_unused_result));

#endif
