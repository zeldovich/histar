extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
#include <inc/taint.h>
#include <inc/memlayout.h>
}

#include <inc/gateclnt.hh>
#include <inc/gateparam.hh>
#include <inc/gateinvoke.hh>
#include <inc/cpplabel.hh>
#include <inc/scopeguard.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
return_stub(struct jmp_buf *jb)
{
    taint_cow();
    longjmp(jb, 1);
}

static void
return_setup(struct cobj_ref *g, struct jmp_buf *jb,
	     void *tls, uint64_t return_handle)
{
    label clear;
    error_check(sys_thread_get_clearance(clear.to_ulabel()));
    clear.set(return_handle, 0);

    label label;
    struct cobj_ref thread_self = COBJ(kobject_id_thread_ct, thread_id());
    error_check(sys_obj_get_label(thread_self, label.to_ulabel()));

    struct thread_entry te;
    te.te_entry = (void *) &return_stub;
    te.te_stack = (char *) tls + PGSIZE;
    te.te_arg = (uint64_t) jb;
    error_check(sys_thread_get_as(&te.te_as));

    uint64_t ct = kobject_id_thread_ct;
    int64_t id = sys_gate_create(ct, &te,
				 label.to_ulabel(), clear.to_ulabel(),
				 "return gate");
    error_check(id);
    *g = COBJ(ct, id);
}

void
gate_call(struct cobj_ref gate, struct cobj_ref *param,
	  label *cs, label *ds, label *dr)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    void *tls = 0;
    error_check(segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &tls, 0));
    scope_guard<int, void *> g(segment_unmap, tls);

    int64_t return_handle = sys_handle_create();
    error_check(return_handle);
    scope_guard<void, uint64_t> g1(thread_drop_star, return_handle);

    struct cobj_ref return_gate;
    struct jmp_buf back_from_call;
    return_setup(&return_gate, &back_from_call, tls, return_handle);
    scope_guard<int, struct cobj_ref> g2(sys_obj_unref, return_gate);

    label new_ds(*ds);
    new_ds.set(return_handle, LB_LEVEL_STAR);

    struct gate_call_data *d = (struct gate_call_data *) tls;
    d->param = *param;
    d->return_gate = return_gate;

    if (setjmp(&back_from_call) == 0)
	gate_invoke(gate, cs, &new_ds, dr);

    *param = d->param;
}
