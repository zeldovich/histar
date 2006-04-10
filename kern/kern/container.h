#ifndef JOS_KERN_CONTAINER_H
#define JOS_KERN_CONTAINER_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <kern/kobjhdr.h>
#include <kern/label.h>
#include <inc/container.h>

struct container_slot {
    kobject_id_t cs_id;
    uint64_t cs_ref;
};

#define NUM_CT_SLOT_PER_PAGE	(PGSIZE / sizeof(struct container_slot))
struct container_page {
    struct container_slot cpg_slot[NUM_CT_SLOT_PER_PAGE];
};

#define NUM_CT_SLOT_INLINE	16
struct Container {
    struct kobject_hdr ct_ko;

    // Number of bytes that this container and its sub-objects are taking up.
    // Must always be less than ko_quota_reserve, except in cases when
    // ko_quota_reserve == CT_QUOTA_INF.
    uint64_t ct_quota_used;

    // Cannot store certain types of objects
    uint64_t ct_avoid_types;

    struct container_slot ct_slots[NUM_CT_SLOT_INLINE];
};

int	container_alloc(const struct Label *l, struct Container **cp)
    __attribute__ ((warn_unused_result));
int	container_gc(struct Container *c)
    __attribute__ ((warn_unused_result));
uint64_t container_nslots(const struct Container *c);

// Find a container with the given ID
int	container_find(const struct Container **cp, kobject_id_t id,
		       info_flow_type iflow)
    __attribute__ ((warn_unused_result));

// Store a reference to the object in the container
int	container_put(struct Container *c, const struct kobject_hdr *ko)
    __attribute__ ((warn_unused_result));

// Get the object in a given container slot
int	container_get(const struct Container *c, kobject_id_t *idp, uint64_t slot)
    __attribute__ ((warn_unused_result));

// Drop a reference to the given object from the container
int	container_unref(struct Container *c, const struct kobject_hdr *ko)
    __attribute__ ((warn_unused_result));

// Find an object by <container-id, object-id> pair
int	cobj_get(struct cobj_ref ref, kobject_type_t type,
		 const struct kobject **storep, info_flow_type iflow)
    __attribute__ ((warn_unused_result));

#endif
