#ifndef JOS_INC_SSLDCLNT_HH
#define JOS_INC_SSLDCLNT_HH

void ssld_taint_cow(struct cobj_ref cow_gate, 
		    struct cobj_ref cipher_biseg, struct cobj_ref plain_biseg,
		    uint64_t root_ct, uint64_t taint, struct thread_args *ta);

struct ssld_cow_args {
    struct cobj_ref cipher_biseg;
    struct cobj_ref plain_biseg;
    uint64_t root_ct;
    int64_t rval;
};

#endif
