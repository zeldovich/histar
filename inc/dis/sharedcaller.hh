#ifndef JOS_INC_DIS_SHAREDCALLER_HH
#define JOS_INC_DIS_SHAREDCALLER_HH

#include <inc/dis/globallabel.hh>

class shared_caller
{
public:
    shared_caller(uint64_t shared_ct);
    ~shared_caller(void);

    int read(uint64_t id, const char *fn, 
	     void *dst, uint64_t offset, uint64_t count);
    int write(uint64_t id, const char *fn, 
	      void *src, uint64_t offset, uint64_t count);
    
    void get_label(uint64_t id, const char *fn, global_label *gl);

    void new_user_gate(global_label *gl);
    
    void set_verify(label *v);
    
private:
    struct cobj_ref user_gate(void);
    uint64_t scratch_container(void);

    label verify_label_;

    uint64_t shared_ct_;

    struct cobj_ref user_gt_;
    struct cobj_ref scratch_ct_;
    uint64_t scratch_taint_;
};

#endif
