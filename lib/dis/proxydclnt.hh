#ifndef PROXYDCLNT_HH_
#define PROXYDCLNT_HH_

extern "C" {
#include <inc/types.h>
}

void proxyd_add_mapping(char *global, uint64_t local, 
                       uint64_t grant, uint8_t grant_level);

int64_t proxyd_get_local(char *global);

int proxyd_get_global(uint64_t local, char *ret);



#endif /*PROXYDCLNT_HH_*/
