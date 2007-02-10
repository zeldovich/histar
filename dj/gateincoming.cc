extern "C" {
#include <inc/gateparam.h>
}

#include <inc/gatesrv.hh>
#include <dj/dis.hh>

class incoming_impl : public djgate_incoming {
 public:
    incoming_impl(ptr<djprot> p) : p_(p) {
	gatesrv_descriptor gd;
	gd.gate_container_ = start_env->shared_container;
	gd.name_ = "djd-incoming";
	gd.func_ = &call_stub;
	gd.arg_ = (void *) this;

	gate_ = gate_create(&gd);
    }

    virtual cobj_ref gate() { return gate_; }

 private:
    static void call_stub(void *arg, gate_call_data *gcd, gatesrv_return *ret) {
	incoming_impl *i = (incoming_impl *) arg;
	i->call(gcd, ret);
    }

    void call(gate_call_data *gcd, gatesrv_return *ret) {
	// XXX potentially need to declassify ourselves here..
	//uint64_t call_taint = gcd->call_taint;
	//uint64_t call_grant = gcd->call_grant;

	//p_->
    }

    ptr<djprot> p_;
    cobj_ref gate_;
};

ptr<djgate_incoming>
dj_gate_incoming(ptr<djprot> p)
{
    return New refcounted<incoming_impl>(p);
}
