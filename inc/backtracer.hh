#ifndef JOS_INC_BACKTRACER_HH
#define JOS_INC_BACKTRACER_HH

extern "C" {
#include <inc/backtrace.h>
}

#define BACKTRACER_SLOTS	64

class backtracer {
public:
    backtracer() {
	depth_ = backtrace(&addrs_[0], BACKTRACER_SLOTS);
    }

    virtual ~backtracer() throw () {}

    int backtracer_depth() const { return depth_; }
    void *backtracer_addr(int n) const { return addrs_[n]; }

private:
    void *addrs_[BACKTRACER_SLOTS];
    int depth_;
};

#endif
