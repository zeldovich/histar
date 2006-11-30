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
    uint64_t netd_ct;
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

// ssld_fd.c
int ssl_accept(int s, uint64_t netd_ct, struct cobj_ref ssld_gate);

// ssld_client.cc
int ssld_call(struct cobj_ref gate, struct ssld_op_args *a);

#endif
