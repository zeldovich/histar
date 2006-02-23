#ifndef JOS_INC_SCOPEGUARD_HH
#define JOS_INC_SCOPEGUARD_HH

template <class T>
class scope_guard {
public:
    scope_guard(void (*cb)(T), T p) : cb_(cb), p_(p), active_(true) {}
    void dismiss() { active_ = false; }

    ~scope_guard() {
	if (active_)
	    cb_(p_);
    }

private:
    void (*cb_) (T);
    T p_;
    bool active_;
};

#endif
