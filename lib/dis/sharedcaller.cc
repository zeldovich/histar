extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <inc/gateparam.h>
#include <inc/fs.h>
#include <inc/dis/share.h>
#include <inc/stdio.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <inc/dis/catdir.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/selftaint.hh>

#include <inc/dis/shareutils.hh>
#include <inc/dis/sharedcaller.hh>

shared_caller::shared_caller(uint64_t shared_ct) 
    : verify_label_(1), shared_ct_(shared_ct), scratch_taint_(0)
{
    user_gt_ = COBJ(0, 0);
    scratch_ct_ = COBJ(0, 0); 
}

shared_caller::~shared_caller(void)
{
    if (scratch_ct_.object) {
	assert(scratch_taint_);
	sys_obj_unref(scratch_ct_);
	thread_drop_star(scratch_taint_);
    }
}

int
shared_caller::read(uint64_t id, const char *fn, 
		    void *dst, uint64_t offset, uint64_t count)
{
    struct gate_call_data gcd;
    struct share_args *args = (struct share_args *)&gcd.param_buf[0];
    args->op = share_observe;
    args->observe.id = id;
    args->observe.offset = offset;
    args->observe.count = count;
    assert((strlen(fn) + 1) < sizeof(args->observe.res.buf));
    strcpy(args->observe.res.buf, fn);
    
    gate_call gc(user_gate(), 0, 0, 0);
    gc.call(&gcd, 0);
    
    void *buf = 0;
    uint64_t bytes = 0;
    error_check(args->ret);
    
    error_check(segment_map(args->ret_obj, 0, SEGMAP_READ, &buf, &bytes, 0));
    int cc = MIN(bytes, count);
    memcpy(dst, buf, cc);
    return cc;
}

int
shared_caller::write(uint64_t id, const char *fn, 
		     void *src, uint64_t offset, uint64_t count)
{
    struct gate_call_data gcd;
    struct share_args *args = (struct share_args *)&gcd.param_buf[0];
    args->op = share_modify;
    args->modify.id = id;
    
    assert((strlen(fn) + 1) < sizeof(args->modify.res.buf));
    strcpy(args->modify.res.buf, fn);

    args->modify.offset = offset;
    args->modify.count = count;
    uint64_t t = handle_alloc();
    scope_guard<void, uint64_t> drop(thread_drop_star, t);
    args->modify.taint = t;

    label l(1);
    l.set(t, 3);
    void *buf = 0;
    struct cobj_ref seg;
    error_check(segment_alloc(start_env->shared_container,
			      count, &seg, &buf,
			      l.to_ulabel(), "write seg"));
    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
    scope_guard<int,struct cobj_ref> seg_unref(sys_obj_unref, seg);
    args->modify.seg = seg;
    memcpy(buf, src, count);
    
    label dl(3);
    dl.set(t, LB_LEVEL_STAR);
    gate_call gc(user_gate(), 0, &dl, 0);

    gc.call(&gcd, &verify_label_);
    
    error_check(args->ret);
    return args->ret;
}

void
shared_caller::get_label(uint64_t id, const char *fn, global_label *gl)
{
    struct share_args args;
    args.op = share_get_label;
    args.get_label.id = id;
    strcpy(args.get_label.res.buf, fn);

    error_check(gate_send(user_gate(), &args, sizeof(args), 0));
    error_check(args.ret);
    gl->serial_is(args.get_label.label);
    return;
}

void
shared_caller::set_verify(label *v)
{
    verify_label_.copy_from(v);
}

uint64_t 
shared_caller::scratch_container(void)
{
    if (!scratch_ct_.object) {
	scratch_taint_ = handle_alloc();
	label l(1);
	l.set(scratch_taint_, 3);
	int64_t ct;
	error_check(ct = sys_container_alloc(start_env->shared_container, 
					     l.to_ulabel(), 
					     "shared", 0, CT_QUOTA_INF));
	scratch_ct_ = COBJ(start_env->shared_container, ct);
    }
    return scratch_ct_.object;
}

void 
shared_caller::new_user_gate(global_label *gl)
{
    struct share_args args;
    args.op = share_user_gate;
    args.user_gate.ct = scratch_container();
    assert(sizeof(args.user_gate.label) >= gl->serial_len());
    memcpy(args.user_gate.label, gl->serial(), gl->serial_len());
    
    label dl(3);
    dl.set(scratch_taint_, LB_LEVEL_STAR);

    error_check(gate_send(user_gate(), &args, sizeof(args), &dl));
    error_check(args.ret);
    user_gt_ = args.user_gate.gt;
}

void 
shared_caller::grant_cat(uint64_t cat)
{
    struct share_args args;
    args.op = share_add_local_cat;
    args.add_local_cat.cat = cat;
    label dl(1);
    dl.set(cat, LB_LEVEL_STAR);
    error_check(gate_send(user_gate(), &args, sizeof(args), &dl));
}

struct cobj_ref
shared_caller::user_gate(void)
{
    if (!user_gt_.object) {
	uint64_t gt;
	error_check(gt = container_find(shared_ct_, kobj_gate, "user gate"));    
	user_gt_ = COBJ(shared_ct_, gt);
    }
    return user_gt_;
}
