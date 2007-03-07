#ifndef JOS_DJ_PERF_HH
#define JOS_DJ_PERF_HH

extern "C" {
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#ifdef JOS_TEST
inline uint64_t
read_tsc(void)
{
        uint32_t a, d;
        __asm __volatile("rdtsc" : "=a" (a), "=d" (d));
        return ((uint64_t) a) | (((uint64_t) d) << 32);
}
#else
#include <machine/x86.h>
#endif
}
#include <vec.h>

class dumpable {
 public:
    virtual void dump() = 0;
    virtual ~dumpable() {}
};

class perf_collection : public dumpable {
 public:
    perf_collection() {}

    void add(dumpable *pc) { v_.push_back(pc); }
    virtual void dump() {
	printf("%-20s %12s %12s %12s\n",
	       "Measurement", "Count", "Average", "Total");
	for (uint32_t i = 0; i < v_.size(); i++)
	    v_[i]->dump();
    }

 private:
    vec<dumpable*> v_;
};

extern perf_collection global_stats;

class perf_counter : public dumpable {
 public:
    perf_counter(const char *name) : name_(name), count_(0), total_(0) {
	global_stats.add(this);
    }

    void sample(uint64_t v) { count_++; total_ += v; }
    virtual void dump() {
	if (!count_)
	    return;

	printf("%-20s %12"PRIu64" %12"PRIu64" %12"PRIu64"\n",
	       name_, count_, total_ / count_, total_);
	count_ = 0;
	total_ = 0;
    }

 private:
    const char *name_;
    uint64_t count_;
    uint64_t total_;
};

class scoped_timer {
 public:
    scoped_timer(perf_counter *pc) : pc_(pc), start_(read_tsc()) {}
    ~scoped_timer() { pc_->sample(read_tsc() - start_); }

 private:
    perf_counter *pc_;
    uint64_t start_;
};

#endif
