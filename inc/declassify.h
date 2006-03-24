#ifndef JOS_INC_EXIT_DECLASS_H
#define JOS_INC_EXIT_DECLASS_H

#include <inc/container.h>

enum declassify_reqtype {
    declassify_exit,
};

struct declassify_args {
    int req;
    union {
	struct {
	    struct cobj_ref status_seg;
	    uint64_t parent_pid;
	} exit;
    };
};

void declassifier(void *, struct gate_call_data *, struct gatesrv_return *);

#endif
