#ifndef JOS_INC_DEVPT_H
#define JOS_INC_DEVPT_H

#include <inc/gatefilesrv.h>

typedef enum {
    pts_op_seg = gf_call_count + 1,
    pts_op_close,
} pts_gate_op;

struct pts_gate_args {
    union {
	struct gatefd_args gfd_args;
	struct {
	    pts_gate_op op;
	    struct cobj_ref arg;
	    int64_t ret;
	} pts_args;
    }; 
};

#endif
