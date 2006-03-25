#ifndef JOS_INC_KOBJ_H
#define JOS_INC_KOBJ_H

#include <inc/types.h>

#define KOBJ_NAME_LEN	32	/* including the terminating NULL */

typedef enum {
    kobj_container,
    kobj_thread,
    kobj_gate,
    kobj_segment,
    kobj_address_space,
    kobj_mlt,
    kobj_netdev,
    kobj_label,
    kobj_dead,

    kobj_ntypes,
    kobj_any
} kobject_type_t;

typedef uint64_t kobject_id_t;
#define kobject_id_thread_sg	((kobject_id_t) 0x4000000000000002UL)

#endif
