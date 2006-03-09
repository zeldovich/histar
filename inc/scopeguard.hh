#ifndef JOS_INC_SCOPEGUARD_HH
#define JOS_INC_SCOPEGUARD_HH

extern "C" {
#include <stdio.h>
}

#include <inc/error.hh>

template <class T>
void delete_obj(T *o) {
    delete o;
}

template <class R, class T>
class scope_guard {
public:
    scope_guard(R (*cb)(T), T p) : cb_(cb), p_(p), active_(true) {}
    void dismiss() { active_ = false; }

    ~scope_guard() {
	if (active_) {
	    try {
		cb_(p_);
	    } catch (std::exception &e) {
		printf("~scope_guard: %s\n", e.what());
	    }
	}
    }

private:
    R (*cb_) (T);
    T p_;
    bool active_;
};

#endif
