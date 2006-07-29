extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/types.h>
#include <inc/gateparam.h>
#include <inc/debug.h>
#include <inc/syscall.h>
#include <inc/dis/share.h>

#include <string.h>
}

#include <inc/cpplabel.hh>

#include <inc/dis/shareutils.hh>

#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>

static const char dbg = 1;

static struct cobj_ref share1_client_gt = COBJ(0,0);

static void __attribute__((noreturn))
bridge_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy_all(&bck, parm);
    share_server_args *args = (share_server_args*) parm->param_buf;

    struct share_args sargs;
    struct fs_inode ino;

    error_check(fs_namei(args->resource, &ino));
    sargs.op = share_grant;
    sargs.grant.obj = ino.obj;
    gate_send(share1_client_gt, &sargs, sizeof(sargs), 0);
    gate_call_data_copy_all(parm, &bck);
    
    try {
	switch(args->op) {
	case share_server_read: {
	    debug_print(dbg, "read %s count %d offset %d", args->resource,
			args->count, args->offset);
	    struct cobj_ref seg;
	    void *buf = 0;
	    error_check(segment_alloc(start_env->shared_container,
				      args->count, &seg, &buf,
				      0, "read seg"));
	    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
	    error_check(fs_pread(ino, buf, args->count, args->offset));
	    args->data_seg = seg;
	    args->ret = 0;
	    break;
	}
	case share_server_write: {
	    void *buf = 0;
	    error_check(segment_map(args->data_seg, 0, SEGMAP_READ, 
				    (void **)&buf, 0, 0));
	    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
	    error_check(fs_pwrite(ino, buf, args->count, args->offset));
	    args->ret = 0;
	    break;
	}
	default:
	    throw basic_exception("unknown op %d", args->op);
	}
    } catch (basic_exception e) {
	cprintf("bridge_gate: %s\n", e.what());
    }
    gr->ret(0, 0, 0);
}

int
main (int ac, char **av)
{
    const char *share0 = "omd";
    const char *share1 = "omd2";
    
    if (ac >= 3) {
	share0 = av[1];
	share1 = av[2];
    }
    
    try {
	label th_l, th_cl;
	thread_cur_label(&th_l);
	thread_cur_clearance(&th_cl);
	struct cobj_ref bridge_gt = gate_create(start_env->shared_container,
						"bridge gate", &th_l, 
						&th_cl, &bridge_gate, 0);
	
	uint64_t ct = start_env->root_container;
	int64_t share0_ct, share1_ct;
	error_check(share0_ct = container_find(ct, kobj_container, share0));
	error_check(share1_ct = container_find(ct, kobj_container, share1));
	
	int64_t share0_admin, share1_admin;
	error_check(share0_admin = container_find(share0_ct, kobj_gate, "admin gate"));
	error_check(share1_admin = container_find(share1_ct, kobj_gate, "admin gate"));

	struct share_args sargs;
	sargs.op = share_add_server;
	sargs.add_principal.id = 1;
	sargs.add_principal.gate = bridge_gt;
	gate_send(COBJ(share0_ct, share0_admin), &sargs, sizeof(sargs), 0);
	
	sargs.op = share_add_client;
	sargs.add_principal.id = 0;
	gate_send(COBJ(share1_ct, share1_admin), &sargs, sizeof(sargs), 0);

	int64_t share1_client;
	error_check(share1_client = container_find(share1_ct, kobj_gate, "client gate"));
	
	share1_client_gt = COBJ(share1_ct, share1_client);
    } catch (basic_exception e) {
	cprintf("main: %s\n", e.what());
	return -1;
    }

    thread_halt();
    return 0;
}
