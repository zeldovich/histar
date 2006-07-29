#ifndef JOS_INC_DIS_SHARE_H
#define JOS_INC_DIS_SHARE_H

typedef enum {
    share_server_read = 1,
    share_server_write,
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
    share_observe = 1,
    share_modify,
    share_add_local_cat,
    // client
    share_grant,
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
	} observe;
	struct {
	    uint64_t id;
	    struct om_res res;
	} modify;
	struct {
	    uint64_t cat;
	} add_local_cat;
	struct {
	    uint64_t id;
	    struct cobj_ref gate;
	} add_principal;
	struct {
	    struct cobj_ref obj;
	    char label[64];
	} grant;
    };
    // return
    int ret;
};


#endif
