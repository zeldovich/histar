extern "C" {
#include <inc/bipipe.h>
#include <inc/lib.h>
#include <inc/pt.h>
#include <inc/error.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/devpt.h>

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>

static int64_t
pt_send(struct cobj_ref gate, void *args, uint64_t n)
{
    struct gate_call_data gcd;
    void *args2 = (void *) &gcd.param_buf[0];
    memcpy(args2, args, n);
    try {
	label ds(3);
	ds.set(start_env->process_grant, LB_LEVEL_STAR);
	gate_call(gate, 0, &ds, 0).call(&gcd, &ds);
    } catch (std::exception &e) {
	cprintf("pt_send:: %s\n", e.what());
	errno = EPERM;
	return -1;
    }
    memcpy(args, args2, n);
    return 0;
}

extern "C" int
pts_open(struct cobj_ref slave_gt, struct cobj_ref seg, int flags)
{
    struct Fd *fds;
    int r;
    r = fd_alloc(&fds, "pts fd");
    if (r < 0) {
	__set_errno(ENOMEM);
	return -1;
    }

    struct bipipe_seg *bs = 0;
    r = segment_map(seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &bs, 0, 0);
    if (r < 0) {
	cprintf("pts_open: cannot segment_map: %s\n", e2s(r));
	jos_fd_close(fds);
	errno = EIO;
	return -1;
    }
    
    uint64_t taint = bs->taint;;
    uint64_t grant = bs->grant;

    fds->fd_pt.bipipe_seg = seg;
    fds->fd_omode = flags;
    fds->fd_pt.bipipe_a = 0;
    fd_set_extra_handles(fds, grant, taint);
    bs->p[0].open = 1;
    
    fds->fd_dev_id = devpt.dev_id;
    fds->fd_isatty = 1;
    fds->fd_pt.gate = slave_gt;
    fds->fd_pt.is_master = 0;
    
    return fd2num(fds);;
}

extern "C" int
ptm_open(struct cobj_ref master_gt, struct cobj_ref slave_gt, int flags)
{
    int r;
    struct cobj_ref seg;
    struct bipipe_seg *bs = 0;
    uint64_t ct = start_env->shared_container;
    
    uint64_t taint = handle_alloc();
    uint64_t grant = handle_alloc();
    struct ulabel *l = label_alloc();
    l->ul_default = 1;
    label_set_level(l, taint, 3, 1);
    label_set_level(l, grant, 0, 1);
    if ((r = segment_alloc(ct, sizeof(*bs), &seg, (void **)&bs, l, "bipipe")) < 0) {
        errno = ENOMEM;
        return -1;        
    }

    memset(bs, 0, sizeof(*bs));

    struct Fd *fdm;
    r = fd_alloc(&fdm, "ptm fd");
    if (r < 0) {
	sys_obj_unref(seg);
	__set_errno(ENOMEM);
	return -1;
    }
    
    fdm->fd_pt.bipipe_seg = seg;
    fdm->fd_omode = flags;
    fdm->fd_pt.bipipe_a = 1;
    fd_set_extra_handles(fdm, grant, taint);
    bs->p[1].open = 1;
    bs->taint = taint;
    bs->grant = grant;
    
    fdm->fd_dev_id = devpt.dev_id;
    fdm->fd_isatty = 1;
    fdm->fd_pt.gate = slave_gt;
    fdm->fd_pt.is_master = 1;
    
    struct pts_gate_args args;
    args.pts_args.op = pts_op_seg;
    args.pts_args.arg = seg;
    if ((pt_send(slave_gt, &args, sizeof(args)) < 0) ||
	(args.pts_args.ret < 0)) {
	jos_fd_close(fdm);
	struct pts_gate_args args2;
	args2.pts_args.op = pts_op_close;
	pt_send(master_gt, &args2, sizeof(args2));
	return -1;
    }
    
    return fd2num(fdm);
}

extern "C" int
pt_pts_no(struct Fd *fd, int *ptyno) 
{
    if (fd->fd_dev_id != devpt.dev_id || !fd->fd_pt.is_master) {
	errno = EBADF;
	return -1;
    }
    
    char name[KOBJ_NAME_LEN];
    int r = sys_obj_get_name(fd->fd_pt.gate, &name[0]);
    if (r < 0) {
	errno = EBADF;
	return -1;
    }
    
    *ptyno = atoi(name);
    return 0;
}

static int
pt_close(struct Fd *fd)
{
    if (!fd->fd_pt.is_master)
	return 0;
    try {
	struct pts_gate_args args;
	args.pts_args.op = pts_op_close;
	error_check(pt_send(fd->fd_pt.gate, &args, sizeof(args)));
	error_check((*devbipipe.dev_close)(fd));
    } catch (basic_exception e) {
	cprintf("pt_close: %s\n", e.what());
	return -1;
    }
    return 0;
}

static ssize_t
pt_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return (*devbipipe.dev_read)(fd, buf, count, offset);
}

static ssize_t
pt_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return (*devbipipe.dev_write)(fd, buf, count, offset);
}

static int
pt_probe(struct Fd *fd, dev_probe_t probe)
{
    return (*devbipipe.dev_probe)(fd, probe);
}

static int
pt_stat(struct Fd *fd, struct stat *buf)
{
    buf->st_mode |= S_IFCHR;
    return 0;
}

static int
pt_shutdown(struct Fd *fd, int how)
{
    return (*devbipipe.dev_shutdown)(fd, how);
}

struct Dev devpt = {
    'y',
    "pt",
    &pt_read,
    &pt_write,    
    0,
    0,
    &pt_close,
    0,
    0,
    &pt_stat,
    &pt_probe,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &pt_shutdown,
};
