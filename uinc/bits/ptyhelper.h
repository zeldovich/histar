#ifndef UCLIBC_JOS64_PTYHELPER_H
#define UCLIBC_JOS64_PTYHELPER_H

#include <inc/jcomm.h>

typedef enum {
    ptyd_op_alloc_pts = 1,
    ptyd_op_remove_pts, 
} ptyd_op_t;

struct pts_descriptor {
    struct cobj_ref slave_pty_seg;
    struct jcomm slave_jc;
    uint64_t grant;
    uint64_t taint;
};

struct pty_args {
    ptyd_op_t op_type;
    
    union {
	struct pts_descriptor alloc;
	struct { int ptyno; } remove;
    };
    
    int ret;
};

int pty_alloc(struct pts_descriptor *pd);
int pty_remove(int ptyno);
int pty_lookup(int ptyno, struct pts_descriptor *pd);

#endif
