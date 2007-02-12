#include <dj/dis.hh>

class dummy_catmgr : public catmgr {
 public:
    virtual uint64_t alloc() {
	return ++c_;
    }

    virtual void release(uint64_t c) {}
    virtual void acquire(const label &l) {}

 private:
    uint64_t c_;
};

ptr<catmgr>
dj_dummy_catmgr()
{
    return New refcounted<dummy_catmgr>();
}
