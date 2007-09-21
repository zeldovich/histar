#ifndef JOS_INC_ADMIND_H
#define JOS_INC_ADMIND_H

#include <inc/container.h>

enum {
    admind_op_get_top,
    admind_op_drop
};

struct admind_req {
    int op;
    struct cobj_ref obj;
};

struct admind_reply {
    int err;
    uint64_t top_ids[3];
    uint64_t top_bytes[3];
};

#endif
