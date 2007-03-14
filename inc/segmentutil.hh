#ifndef JOS_INC_SEGMENTUTIL_HH
#define JOS_INC_SEGMENTUTIL_HH

class segment_helper
{
public:
    segment_helper(cobj_ref seg_obj, void **vap, uint64_t flags) 
    {
	int r =  segment_map(seg_obj, 0, flags, &addr_, &bytes_, 0);
	if (r < 0)
	    throw error(r, "cannot map segment");
	if (vap)
	    *vap = addr_;
	
    }
    ~segment_helper(void) {
	segment_unmap_delayed(addr_, 1);
    }
    
    uint64_t bytes(void) { return bytes_; }
    void *   addr(void) { return addr_; }

private:
    uint64_t bytes_;
    void *addr_;
};

class segment_reader : public segment_helper
{
public:
    segment_reader(cobj_ref seg_obj, void **vap = 0) : 
	segment_helper(seg_obj, vap, SEGMAP_READ) {}
};

class segment_writer : public segment_helper
{
public:
    segment_writer(cobj_ref seg_obj, void **vap = 0) : 
	segment_helper(seg_obj, vap, SEGMAP_READ | SEGMAP_WRITE) {}
};

#endif
