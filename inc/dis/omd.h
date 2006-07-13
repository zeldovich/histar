#ifndef JOS_INC_DIS_OMD_H
#define JOS_INC_DIS_OMD_H

#include <inc/dis/omparam.h>

typedef enum {
    om_observe,
    om_modify,
} om_op_t;

struct om_args
{
    om_op_t op;
    union {
	struct {
	    char t;
	    struct om_res res;
	} observe;
	struct {
	    char t;
	    struct om_res res;
	} modify;
    };
    // return
    int ret;
};

#endif
