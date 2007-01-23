#ifndef JOS_MACHINE_LNXPAGE_H
#define JOS_MACHINE_LNXPAGE_H

#include <machine/types.h>
#include <signal.h>

extern void *physmem_base;
extern int physmem_file_fd;
void lnxpage_init(uint64_t membytes);
void lnxpmap_init(void);
void lnxpmap_prefill(void);

#endif
