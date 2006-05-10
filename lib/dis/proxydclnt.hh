#ifndef JOS_INC_PROXYDCLNT_HH_
#define JOS_INC_PROXYDCLNT_HH_


#ifdef JOS_USER
#include <inc/types.h>
#else
#include <sys/types.h>
#endif // JOS_USER

int proxyd_add_mapping(char *global, uint64_t local, 
                       uint64_t grant, uint8_t grant_level);

int proxyd_get_local(char *global, uint64_t *local);
int proxyd_get_global(uint64_t local, char *ret);

int proxyd_acquire_local(char *global, uint64_t *local);
int proxyd_acquire_global(uint64_t local, char *ret);

#endif /*JOS_INC_PROXYDCLNT_HH_*/
