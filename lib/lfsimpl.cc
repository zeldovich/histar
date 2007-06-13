extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/lfsimpl.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/assert.h>

#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

/* 
 * server code
 */
static void __attribute__((noreturn))
lfs_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    lfs_request_handler h = (lfs_request_handler) a;
    
    uint64_t ct = start_env->proc_container;
    struct cobj_ref arg = gcd->param_obj;

    int64_t arg_copy_id = sys_segment_copy(arg, ct, 0,
					   "lfs_gate_entry() args");
    if (arg_copy_id < 0)
	cprintf("lfs_gate_entry: cannot copy <%"PRIu64".%"PRIu64"> args: %s",
	      arg.container, arg.object, e2s(arg_copy_id));
    sys_obj_unref(arg);

    struct cobj_ref arg_copy = COBJ(ct, arg_copy_id);
    struct lfs_op_args *lfs_op = 0;
    int r = segment_map(arg_copy, 0, SEGMAP_READ | SEGMAP_WRITE, (void**)&lfs_op, 0, 0);
    if (r < 0)
	panic("lfs_gate_entry: cannot map args: %s\n", e2s(r));

    h(lfs_op);
    segment_unmap(lfs_op);

    uint64_t copy_back_ct = gcd->taint_container;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct, 0,
					    "lfs_gate_entry reply");
    if (copy_back_id < 0)
	panic("lfs_gate_entry: cannot copy back: %s", e2s(copy_back_id));

    sys_obj_unref(arg_copy);
    gcd->param_obj = COBJ(copy_back_ct, copy_back_id);
    rg->ret(0, 0, 0);
}

int
lfs_server_init(lfs_request_handler h, struct cobj_ref *gate)
{
    try {
	label l(1);
	label c(3);
	label v(3);
	
	thread_cur_label(&l);
	thread_cur_clearance(&c);	
	
	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.label_ = &l;
	gd.clearance_ = &c;
	gd.verify_ = &v;

	gd.arg_ = (uintptr_t) h;
	gd.name_ = "lfs";
	gd.func_ = &lfs_gate_entry;
	gd.flags_ = 0;//GATESRV_KEEP_TLS_STACK;
	*gate = gate_create(&gd);
    } catch (std::exception &e) {
	cprintf("lfs_server_init: %s\n", e.what());
	return -1;
    }
    return 0;
}

int
lfs_create(struct fs_inode dir, const char *fn, const char *linux_pn, struct cobj_ref gate)
{
    struct fs_inode ino;
    try {
	error_check(fs_mknod(dir, fn, devlfs.dev_id, 0, &ino, 0));
	struct lfs_descriptor ld;
	strncpy(ld.pn, linux_pn, sizeof(ld.pn) - 1);
	ld.pn[sizeof(ld.pn) - 1] = 0;
	ld.gate = gate;
	error_check(fs_pwrite(ino, (void *)&ld, sizeof(ld), 0));
    } catch (std::exception &e) {
	cprintf("lfs_create: %s\n", e.what());
	return -1;
    }
    return 0;
}

/*
 * client code
 */
static int
lfs_call(struct cobj_ref gate, struct lfs_op_args *a)
{
    try {
	gate_call c(gate, 0, 0, 0);
	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(c.call_ct(), sizeof(*a), &seg, &va,
				  0, "lfs_call() args"));
	memcpy(va, a, sizeof(*a));
	segment_unmap(va);

	struct gate_call_data gcd;
	gcd.param_obj = seg;
	c.call(&gcd);
	
	va = 0;
	error_check(segment_map(gcd.param_obj, 0, SEGMAP_READ, &va, 0, 0));
	memcpy(a, va, sizeof(*a));
	segment_unmap(va);
    } catch (std::exception &e) {
	cprintf("lfs_call: %s\n", e.what());
	return -1;
    }

    if (a->rval < 0) {
	errno = -1 * a->rval;
	return -1;
    }
    return a->rval;
}

int
lfs_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "lind-fs");
    if (r < 0) {
        errno = ENOMEM;
        return -1;
    }
    fd->fd_dev_id = devlfs.dev_id;
    fd->fd_omode = flags;
   
    ssize_t s = fs_pread(ino, (void *)&fd->fd_lfs.ld, sizeof(fd->fd_lfs.ld), 0);
    if (s < 0) {
	errno = EPERM;
	return -1;
    }
    return fd2num(fd);
}

ssize_t 
lfs_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    struct lfs_op_args a;
    a.op_type = lfs_op_read;
    strcpy(a.pn, fd->fd_lfs.ld.pn);
    a.bufcnt = MIN(len, sizeof(a.buf));
    a.offset = offset;
    int r = lfs_call(fd->fd_lfs.ld.gate, &a);
    if (r < 0)
	return r;
    memcpy(buf, a.buf, r);
    return r;
}

int 
lfs_close(struct Fd *fd)
{
    return 0;
}
