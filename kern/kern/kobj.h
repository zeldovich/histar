#ifndef JOS_KERN_KOBJ_H
#define JOS_KERN_KOBJ_H

#include <machine/types.h>
#include <kern/label.h>
#include <inc/kobj.h>
#include <inc/queue.h>

typedef uint64_t kobject_id_t;
#define kobject_id_null		((kobject_id_t) -1)

struct kobject {
    kobject_type_t ko_type;
    kobject_id_t ko_id;
    uint64_t ko_ref;
    uint64_t ko_extra_pages;
    struct Label ko_label;
    LIST_ENTRY(kobject) ko_link;
};

LIST_HEAD(kobject_list, kobject);
extern struct kobject_list ko_list;

int  kobject_get(kobject_id_t id, struct kobject **kp);
int  kobject_alloc(kobject_type_t type, struct Label *l, struct kobject **kp);

void kobject_swapin(struct kobject *kp);
void kobject_swapin_page(struct kobject *kp, uint64_t page_num, void *p);
void *kobject_swapout_page(struct kobject *kp, uint64_t page_num);

void kobject_free(struct kobject *kp);
void kobject_incref(struct kobject *kp);
void kobject_decref(struct kobject *kp);

#endif
