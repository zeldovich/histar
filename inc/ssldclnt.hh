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



#endif
