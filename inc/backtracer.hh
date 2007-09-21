#ifndef JOS_INC_BACKTRACER_HH
#define JOS_INC_BACKTRACER_HH

extern "C" {
#include <inc/backtrace.h>
#include <string.h>
}

#define BACKTRACER_SLOTS	64

class backtracer {
 public:
    backtracer()
	: depth_(backtrace(&addrs_[0], BACKTRACER_SLOTS))
    {
    }

    backtracer(const backtracer &o) : depth_(o.depth_) {
	memcpy(&addrs_[0], &o.addrs_[0], sizeof(addrs_));
    }

    virtual ~backtracer() throw () {}

    int backtracer_depth() const { return depth_; }
    void *backtracer_addr(int n) const { return addrs_[n]; }

 private:
    backtracer &operator=(const backtracer&);

    void *addrs_[BACKTRACER_SLOTS];
    int depth_;
};

#endif
