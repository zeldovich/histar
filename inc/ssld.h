#ifndef JOS_INC_SSLD_H
#define JOS_INC_SSLD_H

enum { ssld_buf_size = 8192 };

typedef enum {
    ssld_op_accept,
    ssld_op_send,
    ssld_op_recv,
    ssld_op_close,
} ssld_op_t;

struct ssld_op_accept_args {
    int s;
    struct cobj_ref netd_gate;
};

struct ssld_op_send_args {
    int s;
    uint32_t count;
    int flags;
    char buf[ssld_buf_size];
};

struct ssld_op_recv_args {
    int s;
    uint32_t count;
    int flags;
    char buf[ssld_buf_size];
};

struct ssld_op_close_args {
    int s;
};

struct ssld_op_args {
    ssld_op_t op_type;
    int rval;
    
    union {
	struct ssld_op_accept_args accept;
	struct ssld_op_send_args send;
	struct ssld_op_recv_args recv;
	struct ssld_op_close_args close;
    };
};

int ssl_accept(int s);

// ssld_client.cc
int ssld_call(struct cobj_ref gate, struct ssld_op_args *a);
struct cobj_ref ssld_get_gate(void);

// ssld_gatesrv.cc
int ssld_server_init(uint64_t taint, const char *server_pem, const char *password, 
		     const char *calist_pem, const char *dh_pem);

#endif
