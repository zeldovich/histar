#ifndef JOS_DJ_DJGATESRV_HH
#define JOS_DJ_DJGATESRV_HH

extern "C" {
#include <inc/syscall.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
}

#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <dj/dis.hh>
#include <dj/gateutil.hh>

class djgatesrv {
 public:
    djgatesrv(gatesrv_descriptor *gd, djgate_service_cb srv) : srv_(srv) {
	gd->func_ = &djgatesrv::gate_entry;
	gd->arg_ = this;
	gs_ = gate_create(gd);
    }

    ~djgatesrv() {
	sys_obj_unref(gs_);
    }

    cobj_ref gate() { return gs_; }

 private:
    static void __attribute__((noreturn))
    gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	uint64_t call_taint = gcd->call_taint;
	uint64_t call_grant = gcd->call_grant;

	djcall_args *in = New djcall_args();
	scope_guard<void, djcall_args*> delin(delete_obj, in);

	{ // GC scope
	    label vl, vc;
	    thread_cur_verify(&vl, &vc);
	    dj_gate_call_incoming(gcd->param_obj, vl, vc,
				  call_grant, call_taint,
				  &in->taint, &in->grant, &in->data);
	}

	djgatesrv *dgs = (djgatesrv *) arg;
	djcall_args *out = New djcall_args();
	scope_guard<void, djcall_args*> delout(delete_obj, out);
	if (!dgs->srv_(*in, out))
	    throw basic_exception("djgatesrv: service unhappy\n");

	/* Marshal response back into a segment */
	cobj_ref data_seg;
	void *data_map = 0;
	out->taint.set(call_taint, 3);
	out->taint.set(call_grant, 0);
	error_check(segment_alloc(gcd->taint_container, out->data.len(),
				  &data_seg, &data_map,
				  out->taint.to_ulabel_const(),
				  "djgatesrv reply args"));
	scope_guard2<int, void*, int> unmap2(segment_unmap_delayed, data_map, 1);
	memcpy(data_map, out->data.cstr(), out->data.len());
	unmap2.force();

	label *cs = New label(out->taint);
	label *ds = New label(out->grant);
	label *dr = New label(out->taint);

	label *nvl = New label();
	out->grant.merge(&out->taint, nvl, label::min, label::leq_starlo);
	label *nvc = New label(out->taint);

	nvl->set(gcd->call_taint, LB_LEVEL_STAR);
	nvl->set(gcd->call_grant, LB_LEVEL_STAR);
	nvc->set(gcd->call_taint, 3);
	nvc->set(gcd->call_grant, 3);

	delin.force();
	delout.force();
	gcd->param_obj = data_seg;
	ret->ret(cs, ds, dr, nvl, nvc);
    }

    djgate_service_cb srv_;
    cobj_ref gs_;
};

#endif
