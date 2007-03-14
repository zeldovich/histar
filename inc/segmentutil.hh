#ifndef JOS_INC_SEGMENTUTIL_HH
#define JOS_INC_SEGMENTUTIL_HH

#include <inc/error.hh>

template <class A>
class segment_helper
{
public:
    segment_helper(cobj_ref seg_obj, A **vap, uint64_t flags) 
	: bytes_(0), addr_(0)
    {
	int r = segment_map(seg_obj, 0, flags, (void **)&addr_, &bytes_, 0);
	if (r < 0)
	    throw error(r, "cannot map segment");
	if (vap)
	    *vap = addr_;
    }

    ~segment_helper(void) {
	segment_unmap_delayed(addr_, 1);
    }
    
    uint64_t bytes(void) { return bytes_; }
    A *      addr(void) { return addr_; }

private:
    uint64_t bytes_;
    A *addr_;
};

template <class A>
class segment_reader : public segment_helper<A>
{
public:
    segment_reader(cobj_ref seg_obj, A **vap = 0) : 
	segment_helper<A>(seg_obj, vap, SEGMAP_READ) {}
};

template <class A>
class segment_writer : public segment_helper<A>
{
public:
    segment_writer(cobj_ref seg_obj, A **vap = 0) : 
	segment_helper<A>(seg_obj, vap, SEGMAP_READ | SEGMAP_WRITE) {}
};

#endif
