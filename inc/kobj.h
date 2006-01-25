#ifndef JOS_INC_KOBJ_H
#define JOS_INC_KOBJ_H

#include <inc/types.h>

#define KOBJ_NAME_LEN	32	// including the terminating NULL

typedef enum {
    kobj_container,
    kobj_thread,
    kobj_gate,
    kobj_segment,
    kobj_address_space,

    kobj_netdev,

    kobj_dead,
    kobj_any
} kobject_type_t;

#endif
