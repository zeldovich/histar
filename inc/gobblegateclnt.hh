#ifndef JOS_INC_GOBBLEGATECLNT_HH
#define JOS_INC_GOBBLEGATECLNT_HH

extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>
}

#include <inc/gateclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/error.hh>

class gobblegate_call 
{
public:
    struct leftovers {
	leftovers(void) : 
	    gc_(0), vo_(0), vc_(0) {}

	gate_call *gc_;
	label *vo_;
	label *vc_;

	thread_args ta_;
	gate_call_data gcd_;
	
	void cleanup(void) {
	    if (gc_)
		delete gc_;
	    if (vo_)
		delete vo_;
	    if (vc_)
		delete vc_;
	    if (ta_.container)
		thread_cleanup(&ta_);
	}
    };

    gobblegate_call(cobj_ref gate, 
		   const label *owner, const label *clear,
		   bool cleanup) : cleanup_(cleanup) {
	lo_.gc_ = new gate_call(gate, owner, clear);
    }

    ~gobblegate_call(void) {
	if (cleanup_)
	    lo_.cleanup();
    }

    void call(gate_call_data *gcd_param, const label *vo, const label *vc) {
	cobj_ref t;
	uint64_t thread_ct = lo_.gc_->call_ct();
	memcpy(&lo_.gcd_, gcd_param, sizeof(lo_.gcd_));

	if (vo) {
	    lo_.vo_ = new label();
	    lo_.vo_->add(*vo);
	}
	if (vc) {
	    lo_.vc_ = new label();
	    lo_.vc_->add(*vc);
	}
	
	int r = thread_create_option(thread_ct, &gobblegate_stub,
				     &lo_, sizeof(lo_), 
				     &t, "gobblegate_stub", 
				     &lo_.ta_, THREAD_OPT_ARGCOPY);
	if (r < 0)
	    throw error(r, "cannot start thread");
    }

    uint64_t call_ct(void) { return lo_.gc_->call_ct(); }
    uint64_t call_taint() { return lo_.gc_->call_taint(); }
    uint64_t call_grant() { return lo_.gc_->call_grant(); }

    leftovers lo(void) { return lo_; }

    static void gobblegate_stub(void *a) {
	leftovers *lo = (leftovers *)a;
	lo->gc_->call(&lo->gcd_, lo->vo_, lo->vc_, 0, 0, false);
    }

private:
    leftovers lo_;
    bool cleanup_;
};

#endif
