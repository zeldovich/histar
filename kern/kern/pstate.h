#ifndef JOS_KERN_PSTATE_H
#define JOS_KERN_PSTATE_H

#include <machine/types.h>
#include <kern/kobj.h>
#include <kern/freelist.h>
#include <kern/handle.h>
#include <kern/btree.h>
#include <kern/part.h>

#define PSTATE_MAGIC	0x4A4F535053544154ULL
#define PSTATE_VERSION	3
#define PSTATE_BUF_SIZE	512

struct pstate_header {
    union {
	char ph_buf[PSTATE_BUF_SIZE];

	struct {
	    uint64_t ph_magic;
	    uint64_t ph_version;

	    uint64_t ph_handle_counter;
	    uint64_t ph_user_root_handle;
	    uint64_t ph_user_nsec;
	    uint8_t ph_system_key[SYSTEM_KEY_SIZE];

	    uint64_t ph_sync_ts;
	    uint64_t ph_log_blocks;

	    uint8_t ph_free[sizeof(struct freelist)];
	    uint8_t ph_btrees[BTREE_COUNT * sizeof(struct btree)];
	};
    };
};

void pstate_init(void);
int  pstate_load(void) __attribute__ ((warn_unused_result));

// suspends cur_thread, and wakes it up when it should try again
int  pstate_swapin(kobject_id_t id) __attribute__ ((warn_unused_result));

// suspends cur_thread until a snapshot >= timestamp is taken
int  pstate_sync_user(uint64_t timestamp);
int  pstate_sync_object(uint64_t timestamp, const struct kobject *ko,
			uint64_t start, uint64_t nbytes);
uint64_t pstate_ts_alloc(void);

// schedule a swapout
void pstate_sync(void);

// waits in the kernel until a snapshot is written to disk
int  pstate_sync_now(void);

// checks if there are any pending pstate actions just before return to user
void pstate_op_check(void);

// partition to be used by the pstate code
extern struct part_desc *pstate_part;

#endif
