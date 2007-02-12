#include <dj/dis.hh>

class dummy_catmgr : public catmgr {
 public:
    virtual uint64_t alloc() {
	return ++c_;
    }

    virtual void release(uint64_t c) {}
    virtual void acquire(const label &l, bool droplater, uint64_t e0, uint64_t e1) {}
    virtual void import(const label &l, uint64_t e0, uint64_t e1) {}

 private:
    uint64_t c_;
};

ptr<catmgr>
dj_dummy_catmgr()
{
    return New refcounted<dummy_catmgr>();
}
