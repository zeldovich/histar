#ifndef JOS_KERN_KOBJ_H
#define JOS_KERN_KOBJ_H

#include <machine/types.h>
#include <kern/label.h>
#include <inc/kobj.h>
#include <inc/queue.h>

typedef uint64_t kobject_id_t;
#define kobject_id_null		((kobject_id_t) -1)

#define KOBJ_PIN_IDLE		0x01	// Pinned for the idle process
#define KOBJ_ZERO_REFS		0x02	// Should be in-core for GC

#define KOBJ_DIRECT_PAGES	32
#define KOBJ_PAGES_PER_INDIR	(PGSIZE / sizeof(void*))

struct kobject {
    kobject_type_t ko_type;
    kobject_id_t ko_id;
    uint32_t ko_pin;	// in-memory references (DMA, PTE)
    uint32_t ko_ref;	// persistent references (containers)
    uint64_t ko_flags;
    uint64_t ko_npages;
    struct Label ko_label;
    LIST_ENTRY(kobject) ko_link;

    void *ko_pages[KOBJ_DIRECT_PAGES];
    void **ko_pages_indir1;
};

LIST_HEAD(kobject_list, kobject);
extern struct kobject_list ko_list;

typedef enum {
    iflow_read,			// reading from object
    iflow_write,		// writing to object; will not contaminate
    iflow_write_contaminate,	// writing to object; will contaminate
    iflow_none			// internal/metadata use
} info_flow_type;

int  kobject_get(kobject_id_t id, struct kobject **kpp, info_flow_type iflow);
int  kobject_alloc(kobject_type_t type, struct Label *l, struct kobject **kpp);

int  kobject_set_npages(struct kobject *kp, uint64_t npages);
int  kobject_get_page(struct kobject *kp, uint64_t page_num, void **pp);

// One of the object's extra pages has been brought in from disk.
void kobject_swapin_page(struct kobject *kp, uint64_t page_num, void *p);

// The object has been brought in from disk.
void kobject_swapin(struct kobject *kp);

// Called when the object has been written out to disk and the
// in-memory copy should be discarded.
void kobject_swapout(struct kobject *kp);

// Called by the timer interrupt to garbage-collect free'd kobjects
void kobject_gc_scan();

void kobject_incref(struct kobject *kp);
void kobject_decref(struct kobject *kp);

void kobject_incpin(struct kobject *kp);
void kobject_decpin(struct kobject *kp);

#endif
