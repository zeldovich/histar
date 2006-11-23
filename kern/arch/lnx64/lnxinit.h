#ifndef JOS_MACHINE_LNXINIT_H
#define JOS_MACHINE_LNXINIT_H

#include <machine/types.h>

void lnx64_init(const char *disk_pn, const char *cmdline, uint64_t membytes);

#endif
