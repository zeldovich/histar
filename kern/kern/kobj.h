#ifndef JOS_KERN_KOBJ_H
#define JOS_KERN_KOBJ_H

#include <machine/types.h>
#include <kern/label.h>
#include <inc/kobj.h>
#include <inc/queue.h>

typedef uint64_t kobject_id_t;
#define kobject_id_null		((kobject_id_t) -1)

#define	KOBJ_PIN_IDLE		0x01	// Pinned for the idle process

#define KOBJ_DIRECT_PAGES	32
#define KOBJ_PAGES_PER_INDIR	(PGSIZE / sizeof(void*))

struct kobject {
    kobject_type_t ko_type;
    kobject_id_t ko_id;
    uint64_t ko_ref;
    uint64_t ko_flags;
    uint64_t ko_npages;
    struct Label ko_label;
    LIST_ENTRY(kobject) ko_link;

    void *ko_pages[KOBJ_DIRECT_PAGES];
    void **ko_pages_indir1;
};

LIST_HEAD(kobject_list, kobject);
extern struct kobject_list ko_list;

int  kobject_get(kobject_id_t id, struct kobject **kpp);
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

#endif
