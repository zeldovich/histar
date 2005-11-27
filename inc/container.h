#ifndef JOS_INC_CONTAINER_H
#define JOS_INC_CONTAINER_H

typedef enum {
    cobj_none,
    cobj_container,
    cobj_thread,
    cobj_address_space,
    cobj_gate,

    // not implemented yet
    cobj_segment
} container_object_type;

#endif
