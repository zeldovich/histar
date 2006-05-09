#ifndef JOS_INC_PROXYDCLNT_HH_
#define JOS_INC_PROXYDCLNT_HH_

extern "C" {
#include <inc/types.h>
}

int proxyd_add_mapping(char *global, uint64_t local, 
                       uint64_t grant, uint8_t grant_level);

int proxyd_get_local(char *global, uint64_t *local);

int proxyd_get_global(uint64_t local, char *ret);



#endif /*JOS_INC_PROXYDCLNT_HH_*/
