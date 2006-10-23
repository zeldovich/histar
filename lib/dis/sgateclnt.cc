extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>
#include <inc/assert.h>
#include <inc/syscall.h>

#include <inc/dis/share.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

#include <inc/dis/sgateclnt.hh>

struct cobj_ref
sgate_call::user_gate(void)
{
    if (!user_gt_.object) {
	uint64_t gt;
	error_check(gt = container_find(shared_ct_, kobj_gate, "user gate"));    
	user_gt_ = COBJ(shared_ct_, gt);
    }
    return user_gt_;
}

sgate_call::sgate_call(uint64_t id, const char *pn, uint64_t shared_ct,
		       label *contaminate_label, 
		       label *decontaminate_label, 
		       label *decontaminate_clearance)
{
    shared_ct_ = shared_ct;
    user_gt_ = COBJ(0, 0);
    id_ = id;
    pathname_ = strdup(pn);
    return;
}

sgate_call::~sgate_call(void)
{
    delete pathname_;
}

void
sgate_call::call(sgate_call_data *rgcd,	label *verify)
{
    struct gate_call_data gcd;
    struct share_args *args = (struct share_args *)&gcd.param_buf[0];
    args->op = share_gate_call;
    args->gate_call.id = id_;
    assert((strlen(pathname_) + 1) < sizeof(args->gate_call.pn));
    strcpy(args->gate_call.pn, pathname_);

    uint64_t t = handle_alloc();
    scope_guard<void, uint64_t> drop(thread_drop_star, t);
    args->gate_call.taint = t;
    
    label l(1);
    l.set(t, 3);
    void *buf = 0;
    uint64_t count = sizeof(*rgcd) + rgcd->bytes;
    struct cobj_ref seg;
    error_check(segment_alloc(start_env->shared_container,
			      count, &seg, &buf,
			      l.to_ulabel(), "rgcd seg"));
    scope_guard<int, void*> seg_unmap(segment_unmap, buf);
    scope_guard<int,struct cobj_ref> seg_unref(sys_obj_unref, seg);
    args->gate_call.seg = seg;
    memcpy(buf, rgcd, count);
    
    label dl(3);
    dl.set(t, LB_LEVEL_STAR);
    gate_call gc(user_gate(), 0, &dl, 0);

    gc.call(&gcd, 0);
    
    error_check(args->ret);
}
