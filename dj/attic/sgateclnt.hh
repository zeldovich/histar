#ifndef JOS_INC_DIS_SGATECLNT_HH
#define JOS_INC_DIS_SGATECLNT_HH

#include <dj/sgateparam.h>

class sgate_call
{
public:
    sgate_call(uint64_t id, const char *pn, uint64_t shared_ct,
	       label *contaminate_label, 
	       label *decontaminate_label, 
	       label *decontaminate_clearance);
    ~sgate_call(void);
    
    void call(sgate_call_data *rgcd,
	      label *verify);
private:
    struct cobj_ref user_gate(void);
    struct cobj_ref user_gt_;
    uint64_t shared_ct_;
    
    uint64_t id_;
    char *pathname_; 
};

#endif
