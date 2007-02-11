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
	label *cs = New label(out->taint);
	label *ds = New label(out->grant);
	label *dr = New label(out->taint);
	label *nvl = New label();
	label *nvc = New label();

	dj_gate_call_outgoing(gcd->taint_container, call_grant, call_taint,
			      out->taint, out->grant, out->data,
			      &gcd->param_obj, nvl, nvc);

	delin.force();
	delout.force();
	ret->ret(cs, ds, dr, nvl, nvc);
    }

    djgate_service_cb srv_;
    cobj_ref gs_;
};

#endif
