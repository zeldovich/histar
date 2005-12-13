#ifndef JOS_INC_KOBJ_H
#define JOS_INC_KOBJ_H

#include <inc/types.h>

typedef enum {
    kobj_container,
    kobj_thread,
    kobj_gate,
    kobj_segment,
    kobj_address_space,

    kobj_dead,
    kobj_any
} kobject_type_t;

#endif
