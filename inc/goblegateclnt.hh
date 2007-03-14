#ifndef JOS_INC_GOBLEGATECLNT_HH
#define JOS_INC_GOBLEGATECLNT_HH

extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>
}

#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>

class goblegate_call 
{
public:
    struct leftovers {
	leftovers(void) : 
	    gc_(0), vl_(0), vc_(0) {}
				
	gate_call *gc_;
	label *vl_;
	label *vc_;

	thread_args ta_;
	gate_call_data gcd_;
	
	void cleanup(void) {
	    if (gc_)
		delete gc_;
	    if (vl_)
		delete vl_;
	    if (vc_)
		delete vc_;
	    if (ta_.container)
		thread_cleanup(&ta_);
	}
    };

    goblegate_call(cobj_ref gate, 
		   const label *cs, const label *ds, const label *dr,
		   bool cleanup) : cleanup_(cleanup) {
	lo_.gc_ = new gate_call(gate, cs, ds, dr);
    }

    ~goblegate_call(void) {
	if (cleanup_)
	    lo_.cleanup();
    }

    void call(gate_call_data *gcd_param, const label *vl, const label *vc) {
	cobj_ref t;
	uint64_t thread_ct = lo_.gc_->call_taint_ct();
	memcpy(&lo_.gcd_, gcd_param, sizeof(lo_.gcd_));

	if (vl) {
	    lo_.vl_ = new label();
	    lo_.vl_->from_ulabel(vl->to_ulabel_const());
	}
	if (vc) {
	    lo_.vc_ = new label();
	    lo_.vc_->from_ulabel(vc->to_ulabel_const());
	}
	
	int r = thread_create_option(thread_ct, &goblegate_stub,
				     &lo_, sizeof(lo_), 
				     &t, "goblegate_stub", 
				     &lo_.ta_, THREAD_OPT_ARGCOPY);
	if (r < 0)
	    throw error(r, "cannot start thread");
    }

    uint64_t call_ct(void) { return lo_.gc_->call_ct(); }
    uint64_t call_taint() { return lo_.gc_->call_taint(); }
    uint64_t call_grant() { return lo_.gc_->call_grant(); }
    
    leftovers lo(void) { return lo_; }

    static void goblegate_stub(void *a) {
	leftovers *lo = (leftovers *)a;
	lo->gc_->call(&lo->gcd_, lo->vl_, lo->vc_, 0, 0, false);
    }
    
private:
    leftovers lo_;
    bool cleanup_;
    
};

#endif
