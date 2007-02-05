extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/gatefilesrv.h>
#include <inc/syscall.h>
#include <inc/devpt.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
}

#include <inc/scopeguard.hh>
#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>

# define DEVPTS_SUPER_MAGIC	0x1cd1

static uint64_t pts_ct = 0;
static struct cobj_ref devptm_gt = COBJ(0, 0);

static const uint64_t pts_table_entries = 16;

static struct {
    char inuse;
    uint64_t h_master;
    struct cobj_ref seg;
    struct cobj_ref gate;
} pts_table[pts_table_entries];

static jthread_mutex_t pts_table_mu;

static void __attribute__((noreturn))
pts_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct pts_gate_args *args = (struct pts_gate_args *)parm->param_buf;
    uint64_t id = (uint64_t)arg;
    
    try {
	scoped_jthread_lock lock(&pts_table_mu);
	switch(args->pts_args.op) {
	case gf_call_open: {
	    struct gatefd_args *args2 = (struct gatefd_args *)parm->param_buf;
	    args2->ret.obj0 = pts_table[id].gate;
	    args2->ret.obj1 =  pts_table[id].seg;
	    args2->ret.op = gf_ret_pts;
	    break;
	}
	case pts_op_seg:
	    pts_table[id].seg = args->pts_args.arg;
	    args->pts_args.ret = 0;
	    break;
	case pts_op_close: {
	    struct ulabel *l = label_alloc();
	    scope_guard<void, struct ulabel *> free_l(label_free, l);
	    sys_self_get_verify(l);
	    if (label_get_level(l, pts_table[id].h_master) != LB_LEVEL_STAR) {
		cprintf("pts_gate: invalid verify to close\n");
		args->pts_args.ret = -1;
		break;
	    }
	    struct cobj_ref gt = pts_table[id].gate;
	    memset(&pts_table[id], 0, sizeof(pts_table[id]));
	    error_check(sys_obj_unref(gt));
	    args->pts_args.ret = 0;
	    break;
	}
	default:
	    args->pts_args.ret = -1;
	    break;
	}
    } catch (basic_exception e) {
	cprintf("pts_gate: %s\n", e.what());
	args->pts_args.ret = -1;
    }
    gr->ret(0,0,0);
}

static void
ptm_handle_open(struct gatefd_args *args)
{
    label ver(1);
    thread_cur_verify(&ver);
    
    scoped_jthread_lock lock(&pts_table_mu);
    for (uint64_t i = 0; i < pts_table_entries; i++) {
	if (!pts_table[i].inuse) {
	    pts_table[i].inuse = 1;
	    
	    char buf[32];
	    snprintf(&buf[0], sizeof(buf), "%ld", i);
	    
	    label verify(3);
	    verify.set(start_env->user_grant, 0);

	    struct cobj_ref pts_gt = gate_create(pts_ct, buf, 0, 0, &verify,
						 &pts_gate, (void *) i);
	    pts_table[i].gate = pts_gt;
	    pts_table[i].h_master = args->call.arg;
		    
	    args->ret.obj0 = devptm_gt;
	    args->ret.obj1 = pts_gt;
	    args->ret.op = gf_ret_ptm;
	    return;
	}
    }
    
    args->ret.op = gf_ret_error;
    return;
}

static void __attribute__((noreturn))
ptmx_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct gatefd_args *args = (struct gatefd_args *)parm->param_buf;
    
    switch(args->call.op) {
    case gf_call_open:
	ptm_handle_open(args);
	break;
    default:
	break;
    }
    gr->ret(0, 0, 0);
}

int
main (int ac, char **av)
{
    if (ac < 2) {
	cprintf("usage: %s container [category]\n", av[0]);
	exit(-1);
    }
    uint64_t ct = atol(av[1]);
    memset(pts_table, 0, sizeof(pts_table));
    
    try {
	devptm_gt = gate_create(ct, "ptmx", 0, 0, 0, &ptmx_gate, 0);

	label l(1);
	uint64_t ct2 = 0;
	error_check(ct2 = sys_container_alloc(ct, l.to_ulabel(), "pts", 0, CT_QUOTA_INF));
	pts_ct = ct2;

	struct fs_object_meta m;

	error_check(sys_obj_get_meta(COBJ(ct, pts_ct), &m));
	m.f_type = DEVPTS_SUPER_MAGIC;
	error_check(sys_obj_set_meta(COBJ(ct, pts_ct), 0, &m));
    } catch (basic_exception e) {
	cprintf("devpt fatal: %s\n", e.what());
	exit(-1);
    }
    
    if (ac == 3)
	thread_drop_star(atol(av[2]));
    
    thread_halt();
}
