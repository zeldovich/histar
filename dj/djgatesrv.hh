#ifndef JOS_DJ_DJGATESRV_HH
#define JOS_DJ_DJGATESRV_HH

#include <inc/gatesrv.hh>
#include <dj/dis.hh>

class djgatesrv {
 public:
    djgatesrv(gatesrv_descriptor *gd, djgate_service_cb srv) : srv_(srv) {
	gd.func_ = &djgatesrv::gate_entry;
	gd.arg_ = this;
	gs_ = gate_create(gd);
    }

    ~djgatesrv() {
	sys_obj_unref(gs_);
    }

 private:
    static void gate_entry(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	/* XXX */

	djgatesrv *dgs = (djgatesrv *) arg;
	/* dgs->srv_(..., ...) */
    }

    djgate_service_cb srv_;
    cobj_ref gs_;
};

#endif
