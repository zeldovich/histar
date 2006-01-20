#ifndef JOS_KERN_CONTAINER_H
#define JOS_KERN_CONTAINER_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/kobjhdr.h>
#include <inc/container.h>

struct container_slot {
    kobject_id_t cs_id;
    uint64_t cs_ref;
};

#define NUM_CT_SLOT_PER_PAGE	(PGSIZE / sizeof(struct container_slot))
struct container_page {
    struct container_slot ct_slot[NUM_CT_SLOT_PER_PAGE];
};

struct Container {
    struct kobject_hdr ct_ko;
};

int	container_alloc(struct Label *l, struct Container **cp);
int	container_gc(struct Container *c);
uint64_t container_nslots(const struct Container *c);

// Find a container with the given ID
int	container_find(const struct Container **cp, kobject_id_t id,
		       info_flow_type iflow);

// Store a reference to the object in the container
int	container_put(struct Container *c, struct kobject_hdr *ko);

// Get the object in a given container slot
int	container_get(const struct Container *c, kobject_id_t *idp, uint64_t slot);

// Drop a reference to the given object from the container
int	container_unref(struct Container *c, const struct kobject_hdr *ko);

// Find an object by <container-id, object-id> pair
int	cobj_get(struct cobj_ref ref, kobject_type_t type,
		 const struct kobject **storep, info_flow_type iflow);

#endif
