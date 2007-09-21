extern "C" {
#include <inc/stdio.h>
#include <inc/gateparam.h>

#include <bits/ptyhelper.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>


static struct cobj_ref
get_ptyd(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/ptyd/", &ct_ino));
    uint64_t ptyd_ct = ct_ino.obj.object;
    
    int64_t gate_id;
    error_check(gate_id = container_find(ptyd_ct, kobj_gate, "pty-gate"));
    return COBJ(ptyd_ct, gate_id);
}

static struct cobj_ref
get_pts_seg(void)
{
    struct fs_inode ct_ino;
    error_check(fs_namei("/ptyd/", &ct_ino));
    uint64_t ptyd_ct = ct_ino.obj.object;
    
    int64_t seg_id;
    error_check(seg_id = container_find(ptyd_ct, kobj_segment, "pts-seg"));
    return COBJ(ptyd_ct, seg_id);
}

static int
pty_gatesend(struct pty_args *a)
{
    struct gate_call_data gcd;
    memcpy(gcd.param_buf, a, sizeof(*a));

    try {
	gate_call(get_ptyd(), 0, 0, 0).call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("pty_gatesend: %s\n", e.what());
	return -1;
    }
    memcpy(a, gcd.param_buf, sizeof(*a));
    return 0;
}

int
pty_remove(int ptyno)
{
    struct pty_args a;
    a.op_type = ptyd_op_remove_pts;
    a.remove.ptyno = ptyno;
    if (pty_gatesend(&a) < 0)
	return -1;
    return a.ret;
}

int
pty_alloc(struct pts_descriptor *pd)
{
    struct pty_args a;
    a.op_type = ptyd_op_alloc_pts;
    memcpy(&a.alloc, pd, sizeof(a.alloc));
    if (pty_gatesend(&a) < 0)
	return -1;
    return a.ret;
}

int
pty_lookup(int ptyno, struct pts_descriptor *pd)
{
    try {
	struct pts_descriptor *pts_table = 0;
	cobj_ref seg = get_pts_seg();

	error_check(segment_map(seg, 0, SEGMAP_READ, (void **)&pts_table,
				0, 0));
	
	scope_guard2<int, void *, int> unmap(segment_unmap_delayed, pts_table, 1);
	memcpy(pd, &pts_table[ptyno], sizeof(*pd));
	return 0;
    } catch (basic_exception &e) {
	cprintf("pty_lookup: %s\n", e.what());
	return -1;
    }
    
    return -1;
}
