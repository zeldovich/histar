#ifndef JOS_INC_SSLDCLNT_HH
#define JOS_INC_SSLDCLNT_HH

extern "C" {
#include <inc/jcomm.h>
}

void ssld_taint_cow(cobj_ref cow_gate, jcomm_ref eproc_comm,
		    jcomm_ref cipher_comm, jcomm_ref plain_comm,
		    uint64_t root_ct, uint64_t taint, struct thread_args *ta);

struct ssld_cow_args {
    jcomm_ref cipher_comm;
    jcomm_ref plain_comm;
    jcomm_ref privkey_comm; // optional RSA service
    uint64_t root_ct;
    int64_t rval;
};

void ssl_eproc_taint_cow(cobj_ref gate, jcomm_ref eproc_comm, 
			 uint64_t root_ct, uint64_t taint, thread_args *ta);
    
struct ssl_eproc_cow_args {
    jcomm_ref privkey_comm;
    uint64_t root_ct;
};

#endif
