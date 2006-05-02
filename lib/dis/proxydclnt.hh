#ifndef PROXYDCLNT_HH_
#define PROXYDCLNT_HH_

extern "C" {
#include <inc/types.h>
}

void proxyd_addmapping(char *global, uint64_t local, 
                       uint64_t grant, uint8_t grant_level);

int64_t proxyd_gethandle(char *global);



#endif /*PROXYDCLNT_HH_*/
