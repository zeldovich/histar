extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/declassify.h>

#include <string.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

ssize_t
fs_pwrite(struct fs_inode f, const void *buf, uint64_t count, uint64_t off)
{
    uint64_t cursize;
    int r = fs_getsize(f, &cursize);
    if (r < 0)
	return r;

    uint64_t endpt = off + count;
    if (endpt > cursize) {
	r = fs_resize(f, endpt);
	if (r < 0)
	    return r;

	cursize = endpt;
    }

    char *map = 0;
    r = segment_map(f.obj, SEGMAP_READ | SEGMAP_WRITE, (void **) &map, &cursize);
    if (r < 0)
	return r;

    memcpy(&map[off], buf, count);
    segment_unmap_delayed(map, 1);

    return count;
}

ssize_t
fs_pread(struct fs_inode f, void *buf, uint64_t count, uint64_t off)
{
    char *map = 0;
    uint64_t cursize = 0;
    int r = segment_map(f.obj, SEGMAP_READ, (void **) &map, &cursize);
    if (r < 0)
	return r;

    if (off > cursize)
	count = 0;
    else
	count = MIN(count, cursize - off);

    memcpy(buf, &map[off], count);
    segment_unmap_delayed(map, 1);

    return count;
}

int
fs_getsize(struct fs_inode f, uint64_t *len)
{
    int64_t nbytes = sys_segment_get_nbytes(f.obj);
    if (nbytes < 0)
	return nbytes;

    *len = nbytes;
    return 0;
}

int
fs_resize(struct fs_inode f, uint64_t len)
{
    int r = sys_segment_resize(f.obj, len, 0);
    if (r == -E_LABEL && start_env->declassify_gate.object) {
	try {
	    struct gate_call_data gcd;
	    struct declassify_args *darg =
		(struct declassify_args *) &gcd.param_buf[0];
	    darg->req = declassify_fs_resize;
	    darg->fs_resize.ino = f;
	    darg->fs_resize.len = len;

	    label verify;
	    thread_cur_label(&verify);

	    gate_call(start_env->declassify_gate, &gcd, 0, 0, 0, &verify);
	    r = 0;
	} catch (std::exception &e) {
	    cprintf("fs_resize: declassifying: %s\n", e.what());
	}
    }

    return r;
}
