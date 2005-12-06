#ifndef JOS_KERN_CONTAINER_H
#define JOS_KERN_CONTAINER_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/kobj.h>
#include <inc/container.h>

#define NUM_CT_OBJ_PER_PAGE	(PGSIZE / sizeof(kobject_id_t))
struct container_page {
    kobject_id_t ct_obj[NUM_CT_OBJ_PER_PAGE];
};

struct Container {
    struct kobject ct_ko;
};

int  container_alloc(struct Label *l, struct Container **cp);
int  container_gc(struct Container *c);
int  container_nslots(struct Container *c);

int  container_find(struct Container **cp, kobject_id_t id, info_flow_type iflow);
int  container_put(struct Container *c, struct kobject *ko);
int  container_unref(struct Container *c, uint64_t slot);

int  cobj_get(struct cobj_ref ref, kobject_type_t type, struct kobject **storep, info_flow_type iflow);

#endif
