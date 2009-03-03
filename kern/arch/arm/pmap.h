#ifndef JOS_MACHINE_PMAP_H
#define JOS_MACHINE_PMAP_H

#ifndef __ASSEMBLER__

#include <machine/atag.h>
#include <machine/types.h>

void page_init(struct atag_mem *, int);

#endif /* !__ASSEMBLER__ */

#endif
