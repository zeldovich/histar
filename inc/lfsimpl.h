#ifndef JOS_INC_LFSIMPL_H
#define JOS_INC_LFSIMPL_H

#include <inc/lfs.h>
#include <inc/fd.h>

typedef enum {
    lfs_op_read,
    lfs_op_write,
} lfs_op_t;

struct lfs_op_args {
    lfs_op_t op_type;
    int rval;
    char pn[128];
    char buf[4096];
    int bufcnt;
    int offset;
};

typedef void (*lfs_request_handler)(struct lfs_op_args*);
int lfs_server_init(lfs_request_handler h, struct cobj_ref *gate);
int lfs_create(struct fs_inode dir, const char *fn, 
	       const char *linux_pn, struct cobj_ref gate);

int lfs_open(struct fs_inode ino, int flags, uint32_t dev_opt);
int lfs_close(struct Fd *fd);
ssize_t lfs_read(struct Fd *fd, void *buf, size_t len, off_t offset);

#endif
