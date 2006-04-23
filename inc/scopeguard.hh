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
    ~scope_guard() { force(); }

    void force() {
	if (active_) {
	    dismiss();
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

template <class R, class T1, class T2>
class scope_guard2 {
public:
    scope_guard2(R (*cb)(T1, T2), T1 p1, T2 p2) : cb_(cb), p1_(p1), p2_(p2), active_(true) {}
    void dismiss() { active_ = false; }
    ~scope_guard2() { force(); }

    void force() {
	if (active_) {
	    dismiss();
	    try {
		cb_(p1_, p2_);
	    } catch (std::exception &e) {
		printf("~scope_guard: %s\n", e.what());
	    }
	}
    }

private:
    R (*cb_) (T1, T2);
    T1 p1_;
    T2 p2_;
    bool active_;
};

#endif
