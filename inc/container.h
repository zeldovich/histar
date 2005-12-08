#ifndef JOS_INC_CONTAINER_H
#define JOS_INC_CONTAINER_H

#include <inc/types.h>

struct cobj_ref {
    uint64_t container;
    uint64_t slot;
};

#define COBJ(container, slot) \
	((struct cobj_ref) { (container), (slot) } )

#endif
