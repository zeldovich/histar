#ifndef JOS_KERN_CONTAINER_H
#define JOS_KERN_CONTAINER_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <inc/queue.h>
#include <inc/container.h>
#include <kern/label.h>

struct container_hdr {
    uint64_t idx;
    struct Label *label;
    LIST_ENTRY(Container) link;
};

struct container_object {
    container_object_type type;
    void *ptr;
};

#define NUM_CT_OBJ  ((PGSIZE - sizeof(struct container_hdr)) / sizeof(struct container_object))

struct Container {
    struct container_hdr ct_hdr;
    struct container_object ct_obj[NUM_CT_OBJ];
};

LIST_HEAD(Container_list, Container);

int  container_alloc(struct Container **cp);

// on success, container_put(cobj_container) assumes ownership of the sub-container
int  container_put(struct Container *c, container_object_type type, void *ptr);

void container_unref(struct Container *c, uint32_t idx);
void container_free(struct Container *c);
struct container_object *container_get(struct Container *c, uint32_t idx);

struct Container *container_find(uint64_t cidx);

#endif
