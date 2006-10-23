extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/types.h>
#include <inc/gateparam.h>
#include <inc/debug.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/dis/share.h>
#include <inc/dis/catdir.h>

#include <string.h>
}

#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>

#include <inc/dis/shareutils.hh>
#include <inc/dis/globallabel.hh>
#include <inc/dis/labeltrans.hh>

static const char dbg = 1;

static struct cobj_ref share1_client_gt = COBJ(0,0);
static uint64_t share1_cats_ct;

label_trans *trans1;

static void 
get_global(uint64_t cat, global_cat *gcat)
{
    int64_t seg_id;
    error_check(seg_id = container_find(share1_cats_ct, kobj_segment, "catdir"));

    struct catdir *catdir = 0;
    error_check(catdir_map(COBJ(share1_cats_ct, seg_id), SEGMAP_READ, 
			   &catdir, 0));
    scope_guard<int, void*> seg_unmap(segment_unmap, catdir);

    error_check(catdir_lookup_global(catdir, cat, gcat));
}

level_t
label_cs_diff(level_t a, level_t b, level_comparator leq)
{
    return (leq(a, b) < 0) ? a : LB_LEVEL_STAR;
}

level_t
label_ds_diff(level_t a, level_t b, level_comparator leq)
{
    return (leq(a, b) == 0 && a != b) ? a : 3;
}

level_t
label_dr_diff(level_t a, level_t b, level_comparator leq)
{
    return (leq(a, b) == 0 && a != b) ? a : 0;
}

static void
glabel_to_label(global_label *gl, label *l, char save_gate_args)
{
    try {
	trans1->local_for(gl, l);
    } catch (error &e) {
	if (e.err() == -E_NOT_FOUND) {
	    trans1->localize(gl);
	    trans1->local_for(gl, l);
	} else
	    throw e;
    }
}

static void __attribute__((noreturn))
bridge_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy_all(&bck, parm);
    share_server_args *args = (share_server_args*) parm->param_buf;

    struct share_args sargs;
    struct fs_inode ino;

    try {
	error_check(fs_namei(args->resource, &ino));
	if (args->op == share_server_read ||
	    args->op == share_server_write) {
	    
	    struct ulabel *ul = label_alloc();
	    scope_guard<void, struct ulabel*> free_ul(label_free, ul);
	    error_check(sys_obj_get_label(ino.obj, ul));
	    global_label gl(ul, get_global);

	    sargs.op = share_grant_label;
	    assert(sizeof(sargs.grant.label) >= gl.serial_len());
	    memcpy(sargs.grant.label, gl.serial(), gl.serial_len());
	    gate_send(share1_client_gt, &sargs, sizeof(sargs), 0);
	    gate_call_data_copy_all(parm, &bck);
	}
	
	switch(args->op) {
	case share_server_read: {
	    debug_print(dbg, "read %s count %d offset %d", args->resource,
			args->count, args->offset);
	    int64_t cc;
	    error_check(cc = sys_segment_get_nbytes(ino.obj));
	    if (cc < args->offset) {
		args->ret = -1;
		break;
	    }
	    cc = MIN(args->count, cc - args->offset);
	    
	    struct cobj_ref seg;
	    void *buf = 0;
	    error_check(segment_alloc(parm->taint_container,
				      cc, &seg, &buf,
				      0, "read seg"));
	    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
	    error_check(cc = fs_pread(ino, buf, cc, args->offset));

	    args->data_seg = seg;
	    args->ret = 0;

	    label l(1);
	    obj_get_label(ino.obj, &l);
	    global_label gl(l.to_ulabel(), &get_global);

	    assert(sizeof(args->label) > (uint32_t)gl.serial_len());
	    memcpy(args->label, gl.serial(), gl.serial_len());    
	    args->ret = 0;
	    break;
	}
	case share_server_write: {
	    debug_print(dbg, "write %s count %d offset %d", args->resource,
			args->count, args->offset);
	    
	    global_label gl(args->label);
	    debug_print(dbg, "thread global %s", gl.string_rep());

	    label th_l(1);
	    glabel_to_label(&gl, &th_l, 1);

	    debug_print(dbg, "thread local %s", th_l.to_string());

	    label l(1);
	    obj_get_label(ino.obj, &l);
	    debug_print(dbg, "seg local %s", l.to_string());
	    
	    error_check(l.compare(&th_l, label_leq_starhi));
	    error_check(th_l.compare(&l, label_leq_starlo));

	    void *buf = 0;
	    error_check(segment_map(args->data_seg, 0, SEGMAP_READ, 
				    (void **)&buf, 0, 0));
	    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
	    error_check(fs_pwrite(ino, buf, args->count, args->offset));
	    args->ret = 0;
	    break;
	}
	case share_server_label: {
	    debug_print(dbg, "label for %s", args->resource);
	    label l(1);
	    obj_get_label(ino.obj, &l);
	    global_label gl(l.to_ulabel(), &get_global);
	    
	    assert(sizeof(args->label) > (uint32_t)gl.serial_len());
	    memcpy(args->label, gl.serial(), gl.serial_len());    
	    args->ret = 0;
	    break;
	}
	case share_server_gate_call: {
	    debug_print(dbg, "invoke gate %s", args->resource);

	    global_label gl(args->label);
	    debug_print(dbg, "thread acquired global %s", gl.string_rep());

	    label th_l(1);
	    glabel_to_label(&gl, &th_l, 1);
	    	    
	    debug_print(dbg, "thread acquired local %s", th_l.to_string());

	    // need to acquire global cats from a global label...
	    sargs.op = share_grant_label;
	    assert(sizeof(sargs.grant.label) >= gl.serial_len());
	    memcpy(sargs.grant.label, gl.serial(), gl.serial_len());
	    gate_send(share1_client_gt, &sargs, sizeof(sargs), 0);
	    gate_call_data_copy_all(parm, &bck);
	    
	    // XXX set local ds, dc, cs, and verify...
	    
	    // XXX also some way to martial args
	    
	    label th_l0(1);
	    label th_c0(1);
	    thread_cur_label(&th_l0);
	    thread_cur_clearance(&th_c0);

	    debug_print(dbg, "pre gate thread %s", th_l0.to_string());
	    debug_print(dbg, "pre gate thread cl %s", th_c0.to_string());

	    { // force destructor to cleanup handles
		gate_call_data gcd;
		gate_call gc(ino.obj, 0, 0, 0);
		gc.call(&gcd, 0);
		gate_call_data_copy_all(parm, &bck);
	    }
	    
	    label th_l1(1);
	    label th_c1(1);
	    thread_cur_label(&th_l1);
	    thread_cur_clearance(&th_c1);

	    debug_print(dbg, "post gate thread %s", th_l1.to_string());
	    debug_print(dbg, "post gate thread cl %s", th_c1.to_string());
	    
	    label ds(3), dr(0), cs(LB_LEVEL_STAR);
	    th_l1.merge(&th_l0, &ds, label_ds_diff, label::leq_starlo);
	    th_l1.merge(&th_l0, &cs, label_cs_diff, label::leq_starlo);
	    th_c1.merge(&th_c0, &dr, label_dr_diff, label::leq_starlo);

	    debug_print(dbg, "return local cs %s", cs.to_string());
	    debug_print(dbg, "return local ds %s", ds.to_string());
	    debug_print(dbg, "return local dr %s", dr.to_string());

	    global_label gcs(cs.to_ulabel(), &get_global);
	    global_label gds(ds.to_ulabel(), &get_global);
	    global_label gdr(dr.to_ulabel(), &get_global);

	    debug_print(dbg, "return global cs %s", gcs.string_rep());
	    debug_print(dbg, "return global ds %s", gds.string_rep());
	    debug_print(dbg, "return global dr %s", gdr.string_rep());

	    struct cobj_ref seg;
	    struct rgc_return *r= 0;
	    error_check(segment_alloc(parm->taint_container,
				      sizeof(*r), &seg, (void **)&r,
				      0, "rgcd ret"));
	    scope_guard<int, void*> seg_unmap(segment_unmap, r);

	    
	    assert(sizeof(r->ds) >= gds.serial_len());
	    assert(sizeof(r->cs) >= gcs.serial_len());
	    assert(sizeof(r->dr) >= gdr.serial_len());
	    memcpy(r->ds, gds.serial(), gds.serial_len());
	    memcpy(r->cs, gcs.serial(), gcs.serial_len());
	    memcpy(r->dr, gdr.serial(), gdr.serial_len());
	    
	    // XXX martial args back...

	    args->data_seg = seg;
	    args->ret = 0;
	    
	    break;
	} default:
	    throw basic_exception("unknown op %d", args->op);
	}
    } catch (basic_exception e) {
	cprintf("bridge_gate: %s\n", e.what());
	args->ret = -1;
    }
    gr->ret(0, 0, 0);
}

int
main (int ac, char **av)
{
    const char *share0 = "shared0";
    const char *share1 = "shared1";
    
    gate_call_data gcd;
    static_assert(sizeof(struct share_server_args) <= sizeof(gcd.param_buf));
    
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

	int64_t share1_client, share1_cats;
	error_check(share1_client = container_find(share1_ct, kobj_gate, "client gate"));
	error_check(share1_cats = container_find(share1_ct, kobj_container, "cats"));
	
	share1_client_gt = COBJ(share1_ct, share1_client);
	share1_cats_ct = share1_cats;

	int64_t cats_ct = share1_cats;
	int64_t cats_seg;
	error_check(cats_seg = container_find(cats_ct, kobj_segment, "catdir"));
	
	trans1 = new label_trans(COBJ(cats_ct, cats_seg));
	trans1->client_is(share1_client_gt);
    } catch (basic_exception e) {
	cprintf("main: %s\n", e.what());
	return -1;
    }

    thread_halt();
    return 0;
}
