extern "C" {
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>
#include <inc/gate_fd.h>
#include <inc/gateparam.h>
#include <inc/labelutil.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>

extern "C" int
gatefd(struct cobj_ref gate, int flags)
{

    struct Fd *fd;
    int r = fd_alloc(&fd, "gate");
    if (r < 0) {
        errno = ENOMEM;
        return -1;
    }
    
    fd->fd_dev_id = devgate.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_gate.gate = gate;
    memset(fd->fd_gate.buf, 0, sizeof(fd->fd_gate.buf));
    
    return fd2num(fd);
}


static ssize_t
gatefd_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    uint32_t ret = MIN(count, fd->fd_gate.bytes);
    memcpy(buf, fd->fd_gate.buf, ret);
    fd->fd_gate.bytes = 0; 
    return ret;
}

static ssize_t
gatefd_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    struct gate_call_data gcd;
    struct gatefd_args *args = (struct gatefd_args *)&gcd.param_buf[0];
    
    try {
    
	uint64_t h = handle_alloc();
	scope_guard<void, uint64_t> h_drop(thread_drop_star, h);
	label sl(1);
	sl.set(h, 3);
	
	void *va = 0;
	struct cobj_ref arg_seg;
	error_check(segment_alloc(start_env->shared_container,
				  count, &arg_seg, &va,
				  sl.to_ulabel(), "gate args"));
	scope_guard<int, void*> seg_unmap(segment_unmap, va);
	scope_guard<int, cobj_ref> seg_unref(sys_obj_unref, arg_seg);
	memcpy(va, buf, count);
	
	char *jos_cs = getenv("JOS_CS");
	char *jos_ds = getenv("JOS_DS");
	char *jos_dr = getenv("JOS_DR");
	
	label cs(LB_LEVEL_STAR);
	label ds(3);
	label dr(0);
		
	if (jos_cs)
	    cs.copy_from(jos_cs);
	if (jos_ds)
	    ds.copy_from(jos_ds);
	if (jos_dr)
	    dr.copy_from(jos_dr);

	args->gfd_magic = GATEFD_MAGIC;
	args->gfd_seg = arg_seg;
	args->gfd_ret = 0;
	
	ds.set(h, LB_LEVEL_STAR);
	gate_call(fd->fd_gate.gate, 0, &ds, 0).call(&gcd, 0);

	if (args->gfd_ret < 0)
	    return args->gfd_ret;
	
	uint64_t n = 0;
	void *va2 = 0;
	error_check(segment_map(args->gfd_seg, 0, SEGMAP_READ, &va2, &n, 0));
	scope_guard<int, void*> seg_unmap2(segment_unmap, va2);
	scope_guard<int, cobj_ref> seg_unref2(sys_obj_unref, args->gfd_seg);
	
	uint64_t max = sizeof(fd->fd_gate.buf);
	if (n > max) {
	    cprintf("gatefd_write: trucating response by %ld\n", n - max);
	    n = max;
	}
	memcpy(fd->fd_gate.buf, va2, n);
	fd->fd_gate.bytes = n;
	
	return count;    
    } catch (std::exception &e) {
	cprintf("gatefd_write: gate_call: %s\n", e.what());
	errno = EPERM;
	return -1;
    }    
}

static int
gatefd_close(struct Fd *fd)
{
    return 0;
}
 
struct Dev devgate = {
    'g',
    "gate",
    &gatefd_read,
    &gatefd_write,    
    0,
    0,
    &gatefd_close,
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
    0,
    0,
    0,
    0,
    0,
};
