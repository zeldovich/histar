#ifndef JOS_INC_AUTHCLNT_HH
#define JOS_INC_AUTHCLNT_HH

int auth_call(int op, const char *user, const char *pass, const char *npass,
	       uint64_t *ut, uint64_t *ug);

#endif
