#ifndef JOS_INC_KOBJ_H
#define JOS_INC_KOBJ_H

#include <inc/types.h>
#include <inc/intmacro.h>

#define KOBJ_NAME_LEN	32	/* including the terminating NULL */
#define KOBJ_META_LEN	64

enum kobject_type_enum {
    kobj_container,
    kobj_thread,
    kobj_gate,
    kobj_segment,
    kobj_address_space,
    kobj_netdev,
    kobj_label,
    kobj_dead,

    kobj_ntypes,
    kobj_any
};

typedef uint64_t kobject_id_t;
#define kobject_id_thread_sg	((kobject_id_t) UINT64(0x4000000000000002))

#endif
