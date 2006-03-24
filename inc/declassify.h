#ifndef JOS_INC_EXIT_DECLASS_H
#define JOS_INC_EXIT_DECLASS_H

#include <inc/container.h>
#include <inc/fs.h>

enum declassify_reqtype {
    declassify_exit,
    declassify_fs_create,
};

struct declassify_args {
    int req;
    int status;

    union {
	struct {
	    struct cobj_ref status_seg;
	    uint64_t parent_pid;
	} exit;

	struct {
	    struct fs_inode dir;
	    char name[KOBJ_NAME_LEN];
	    struct fs_inode new_file;
	} fs_create;
    };
};

void declassifier(void *, struct gate_call_data *, struct gatesrv_return *);

#endif
