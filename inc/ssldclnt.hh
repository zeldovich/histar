#ifndef JOS_INC_SSLDCLNT_HH
#define JOS_INC_SSLDCLNT_HH

struct cobj_ref ssld_shared_server(void);
struct cobj_ref ssld_new_server(uint64_t ct, const char *server_pem, 
				const char *password, const char *calist_pem, 
				const char *dh_pem, label *cs, label *ds, 
				label *cr, label *dr, label *co);

struct cobj_ref ssld_shared_cow(void);
struct cobj_ref ssld_cow_call(struct cobj_ref gate, uint64_t ct, 
			      label *cs, label *ds, label *dr);

void ssld_taint_cow(struct cobj_ref cow_gate, 
		    struct cobj_ref cipher_biseg, struct cobj_ref plain_biseg,
		    uint64_t root_ct, uint64_t taint);

struct ssld_cow_op {
    struct cobj_ref cipher_biseg;
    struct cobj_ref plain_biseg;
    uint64_t root_ct;
    int64_t rval;
};

#endif
