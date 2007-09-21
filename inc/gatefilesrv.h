#ifndef JOS_INC_GATEFILESRV_H
#define JOS_INC_GATEFILESRV_H

typedef enum {
    gf_call_open = 1,
    gf_call_count,  // number of calls
} gatefd_call_t;

typedef enum {
    gf_ret_error = 1,
    gf_ret_null,
    gf_ret_ptm,
    gf_ret_pts,
    gf_ret_count, // number of rets
} gatefd_ret_t;

struct gatefd_args 
{
    union {
	struct {
	    gatefd_call_t op;
	    uint64_t      arg;
	} call;
	struct {
	    gatefd_ret_t  op;
	    struct cobj_ref obj0;
	    struct cobj_ref obj1;
	} ret;
    };
} ;

#endif
