#ifndef JOS_INC_AUTHCLNT_HH
#define JOS_INC_AUTHCLNT_HH

void auth_login(const char *user, const char *pass, uint64_t *ug, uint64_t *ut);
void auth_chpass(const char *user, const char *pass, const char *npass);

void auth_log(const char *msg);

#endif
