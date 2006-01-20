#ifndef JOS_KERN_KOBJHDR_H
#define JOS_KERN_KOBJHDR_H

#include <machine/types.h>
#include <kern/label.h>
#include <inc/kobj.h>
#include <inc/queue.h>

typedef uint64_t kobject_id_t;
#define kobject_id_null		((kobject_id_t) -1)

#define KOBJ_PIN_IDLE		0x01	// Pinned for the idle process
#define KOBJ_ZERO_REFS		0x02	// Should be in-core for GC
#define KOBJ_SNAPSHOTING	0x04	// Being written out to disk
#define KOBJ_DIRTY		0x08	// Modified since last swapin/out

#define KOBJ_DIRECT_PAGES	32
#define KOBJ_PAGES_PER_INDIR	(PGSIZE / sizeof(void*))

struct kobject_hdr {
    kobject_type_t ko_type;
    kobject_id_t ko_id;
    uint32_t ko_pin;	// in-memory references (DMA, PTE)
    uint32_t ko_ref;	// persistent references (containers)
    uint64_t ko_flags;
    uint64_t ko_npages;
    struct Label ko_label;
    LIST_ENTRY(kobject_hdr) ko_link;
    char ko_name[KOBJ_NAME_LEN];

    void *ko_pages[KOBJ_DIRECT_PAGES];
    void **ko_pages_indir1;
};

typedef enum {
    iflow_read,
    iflow_rw,
    iflow_write,
    iflow_none
} info_flow_type;

typedef enum {
    kobj_ro,
    kobj_rw
} kobj_rw_mode;

struct kobject;

#endif
