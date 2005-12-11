#ifndef JOS_INC_CONTAINER_H
#define JOS_INC_CONTAINER_H

#include <inc/types.h>

struct cobj_ref {
    uint64_t container;
    uint64_t object;
};

#define COBJ(container, object) \
	((struct cobj_ref) { (container), (object) } )

#endif
