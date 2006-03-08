#ifndef JOS_INC_AUTHCLNT_HH
#define JOS_INC_AUTHCLNT_HH

void auth_call(int op, const char *user, const char *pass,
	       uint64_t *ut, uint64_t *ug);

#endif
