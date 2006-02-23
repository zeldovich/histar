#ifndef JOS_KERN_KOBJHDR_H
#define JOS_KERN_KOBJHDR_H

#include <machine/types.h>
#include <kern/label.h>
#include <kern/pagetree.h>
#include <inc/kobj.h>
#include <inc/queue.h>

#define KOBJ_PIN_IDLE		0x01	// Pinned for the idle process
#define KOBJ_SNAPSHOTING	0x04	// Being written out to disk
#define KOBJ_DIRTY		0x08	// Modified since last swapin/out
#define KOBJ_SNAPSHOT_DIRTY	0x10	// Dirty if swapout fails
#define KOBJ_LABEL_MUTABLE	0x20	// Label can change after creation

struct kobject_hdr {
    kobject_id_t ko_id;
    kobject_type_t ko_type;

    uint64_t ko_ref;	// persistent references (containers)

    uint64_t ko_flags;
    uint64_t ko_nbytes;
    uint64_t ko_min_bytes;	// cannot shrink below this size
    struct Label ko_label;
    char ko_name[KOBJ_NAME_LEN];

    // For verifying the persistence layer
    uint64_t ko_cksum;
    uint64_t ko_gen;

    // Ephemeral state (doesn't persist across swapout)
    uint32_t ko_pin_pg;	// pages are pinned (DMA, PTE)
    uint32_t ko_pin;	// header is pinned (linked lists)

    LIST_ENTRY(kobject_hdr) ko_link;
    LIST_ENTRY(kobject_hdr) ko_hash;

    struct pagetree ko_pt;
};

typedef enum {
    iflow_read,
    iflow_rw,
    iflow_write,
    iflow_none
} info_flow_type;

struct kobject;

#endif
