extern "C" {
#include <inc/stdio.h>
#include <inc/fs.h>

#include <bits/ptyhelper.h>

#include <string.h>
}

#include <inc/scopeguard.hh>
#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>
#include <inc/cpplabel.hh>

static const char *pts_pn = "/dev/pts";

static const uint32_t num_pts = 16;
struct pts_descriptor *pts_table;

static int
alloc_pts(struct pty_args *args)
{
    struct fs_inode pts_ino, num_ino;
    error_check(fs_namei(pts_pn, &pts_ino));

    for (uint32_t i = 0; i < num_pts; i++) {
	if (!pts_table[i].slave_pty_seg.object) {
	    label l(1);
	    
	    pts_table[i].slave_pty_seg = args->alloc.slave_pty_seg;
	    pts_table[i].slave_bipipe_seg = args->alloc.slave_bipipe_seg;
	    pts_table[i].taint = args->alloc.taint;
	    pts_table[i].grant = args->alloc.grant;

	    char num_buf[32];
	    sprintf(num_buf, "%d", i);
	    error_check(fs_mknod(pts_ino, num_buf, 'z', i, 
				 &num_ino, l.to_ulabel()));
	    return i;
	}
    }
    return -1;
}

static int
remove_pts(struct pty_args *args)
{
    uint32_t i = args->remove.ptyno;
    memset(&pts_table[i], 0, sizeof(pts_descriptor));
    
    struct fs_inode pts_ino, num_ino;
    error_check(fs_namei(pts_pn, &pts_ino));
    
    char num_buf[32];
    sprintf(num_buf, "%d", i);
    
    int64_t seg_id, ct_id;
    ct_id = pts_ino.obj.object;
    error_check(seg_id = container_find(ct_id, kobj_segment, num_buf));
    num_ino.obj = COBJ(ct_id, seg_id);
    error_check(fs_remove(pts_ino, num_buf, num_ino));
    return 0;
}

static void __attribute__((noreturn))
pty_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct pty_args *a = (struct pty_args *)parm->param_buf;
    
    try {
	switch (a->op_type) {
	case ptyd_op_alloc_pts:
	    a->ret = alloc_pts(a);
	    break;
	case ptyd_op_remove_pts:
	    a->ret = remove_pts(a);
	    break;
	default:
	    cprintf("pty_gate: unknown op: %d\n", a->op_type);
	    a->ret = -1;
	}
    } catch (basic_exception &e) {
	cprintf("pty_gate: %s\n", e.what());
	a->ret = -1;
    }
    gr->ret(0, 0, 0);
}

int
main(int ac, char **av)
{
    cobj_ref pts_seg;
    label l(1);
    l.set(start_env->process_grant, 0);
    pts_table = 0;
    int r = segment_alloc(start_env->shared_container, 
			  sizeof(pts_descriptor) * num_pts,
			  &pts_seg, (void **) &pts_table, 
			  l.to_ulabel(), "pts-seg");
    if (r < 0) {
	printf("unable to alloc pts_table: %s\n", e2s(r));
	return -1;
    }
    
    // XXX should have sshdi allocate a grant handle g, spawn sshd 
    // with g, and spawn ptyd with g, which can set the verify on the 
    // gate.  
    gate_create(start_env->shared_container, "pty-gate", 
		0, 0, 0, &pty_gate, 0);
    return 0;
}
