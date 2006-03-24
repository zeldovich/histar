#ifndef JOS_INC_AUTHCLNT_HH
#define JOS_INC_AUTHCLNT_HH

int auth_call(int op, const char *user, const char *pass, const char *npass,
	      authd_reply *r);

#endif
