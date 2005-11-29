#ifndef JOS_INC_CONTAINER_H
#define JOS_INC_CONTAINER_H

#include <inc/types.h>

typedef enum {
    cobj_none,
    cobj_container,
    cobj_thread,
    cobj_address_space,
    cobj_gate,
    cobj_segment
} container_object_type;

struct cobj_ref {
    uint64_t container;
    uint64_t idx;
};

#define COBJ(container, idx)	((struct cobj_ref) { (container), (idx) } )

#endif
