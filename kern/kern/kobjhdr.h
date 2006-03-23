#ifndef JOS_KERN_KOBJHDR_H
#define JOS_KERN_KOBJHDR_H

#include <machine/types.h>
#include <inc/kobj.h>
#include <inc/queue.h>
#include <inc/safetype.h>

enum {
    kolabel_contaminate,
    kolabel_clearance,
    kolabel_max
};

#define KOBJ_PIN_IDLE		0x01	// Pinned for the idle process
#define KOBJ_ON_DISK		0x02	// Might have some version on disk
#define KOBJ_SNAPSHOTING	0x04	// Being written out to disk
#define KOBJ_DIRTY		0x08	// Modified since last swapin/out
#define KOBJ_SNAPSHOT_DIRTY	0x10	// Dirty if swapout fails
#define KOBJ_LABEL_MUTABLE	0x20	// Label can change after creation

struct kobject_hdr {
    kobject_id_t ko_id;
    kobject_type_t ko_type;

    uint64_t ko_ref;	// persistent references (containers)

    // Bytes reserved for this object.  Counts towards the parent container's
    // ct_quota_used, and represents the space that is taken up (or could be
    // taken up, without information flow) by this object.
    //
    // For container objects, the space represented by ko_quota_reserve is
    // used to provide space for objects in the container (by incrementing
    // the container's ct_quota_used).
    uint64_t ko_quota_reserve;

    uint64_t ko_flags;
    uint64_t ko_nbytes;
    uint64_t ko_min_bytes;		// cannot shrink below this size
    uint64_t ko_label[kolabel_max];	// id of label object (holds refcount)
    char ko_name[KOBJ_NAME_LEN];

    // For verifying the persistence layer
    uint64_t ko_cksum;

    // Ephemeral state (doesn't persist across swapout)
    uint32_t ko_pin_pg;	// pages are pinned (DMA, PTE)
    uint32_t ko_pin;	// header is pinned (linked lists)
};

typedef SAFE_TYPE(int) info_flow_type;
#define iflow_read  SAFE_WRAP(info_flow_type, 1)
#define iflow_rw    SAFE_WRAP(info_flow_type, 2)
#define iflow_none  SAFE_WRAP(info_flow_type, 3)
#define iflow_alloc SAFE_WRAP(info_flow_type, 4)

struct kobject;

#endif
