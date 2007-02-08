#ifndef JOS_DJ_DJGATESRV_HH
#define JOS_DJ_DJGATESRV_HH

extern "C" {
#include <inc/syscall.h>
#include <inc/gateparam.h>
}

#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <dj/dis.hh>

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
	djcall_args *in = New djcall_args();
	scope_guard<void, djcall_args*> delin(delete_obj, in);

	{ // GC scope
	    label vl, vc;
	    thread_cur_verify(&vl, &vc);
	    cobj_ref seg = gcd->param_obj;

	    /* Read request and labels */
	    label l;
	    obj_get_label(COBJ(seg.container, seg.container), &l);
	    error_check(vl.compare(&l, label::leq_starlo));
	    error_check(l.compare(&vc, label::leq_starhi));
	    obj_get_label(seg, &l);
	    error_check(vl.compare(&l, label::leq_starlo));
	    error_check(l.compare(&vc, label::leq_starhi));

	    in->taint = l;
	    in->grant = vl;
	    in->grant.set(gcd->call_taint, 3);
	    in->grant.set(gcd->call_grant, 3);
	    in->grant.transform(label::nonstar_to, 3);

	    void *data_map = 0;
	    uint64_t data_len = 0;
	    error_check(segment_map(seg, 0, SEGMAP_READ,
				    &data_map, &data_len, 0));
	    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
	    in->data = str((const char *) data_map, data_len);
	    unmap.force();
	}

	djgatesrv *dgs = (djgatesrv *) arg;
	djcall_args *out = New djcall_args();
	scope_guard<void, djcall_args*> delout(delete_obj, out);
	if (!dgs->srv_(*in, out))
	    throw basic_exception("djgatesrv: service unhappy\n");

	/* Marshal response back into a segment */
	cobj_ref data_seg;
	void *data_map = 0;
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

	delin.force();
	delout.force();
	gcd->param_obj = data_seg;
	ret->ret(cs, ds, dr, nvl, nvc);
    }

    djgate_service_cb srv_;
    cobj_ref gs_;
};

#endif
