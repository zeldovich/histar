#ifndef JOS_INC_DIS_SHARE_H
#define JOS_INC_DIS_SHARE_H

struct rgc_return
{
    char cs[32];
    char ds[32];
    char dr[32];
};

typedef enum {
    share_server_read = 1,
    share_server_write,
    share_server_label, 
    share_server_gate_call,
} share_server_op_t;

struct share_server_args
{
    share_server_op_t op;
    int offset;
    int count;

    char resource[32];
    char label[32];
    struct cobj_ref data_seg;
    int64_t ret;
};

struct om_res {
    char buf[32];
};

struct palid {
    int id;
};

typedef enum {
    // user
    share_observe = 1,
    share_modify,
    share_get_label,
    share_user_gate,
    share_add_local_cat,
    share_gate_call,
    // client
    share_grant_label,
    share_localize,
    // admin
    share_add_server,
    share_add_client,
} share_op_t;

struct share_args
{
    share_op_t op;
    union {
	struct {
	    uint64_t id;
	    struct om_res res;
	    uint64_t offset;
	    uint64_t count;
	} observe;
	struct {
	    uint64_t id;
	    struct om_res res;
	    uint64_t offset;
	    uint64_t count;
	    uint64_t taint;
	    struct cobj_ref seg;
	} modify;
	struct {
	    uint64_t cat;
	} add_local_cat;
	struct {
	    uint64_t id;
	    struct cobj_ref gate;
	} add_principal;
	struct {
	    char label[64];
	} grant;
	struct {
	    char label[64];
	} localize;
	struct {
	    uint64_t id;
	    struct om_res res;
	    char label[32];
	} get_label;
	struct {
	    char label[32];
	    uint64_t ct;
	    struct cobj_ref gt;
	} user_gate;
	struct {
	    uint64_t id;
	    char pn[32];
	    uint64_t taint;
	    struct cobj_ref seg;
	} gate_call;
    };
    // return
    int ret;
    struct cobj_ref ret_obj; 
};


#endif
