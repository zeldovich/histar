#ifndef JOS_MACHINE_TAG_H
#define JOS_MACHINE_TAG_H

#define TAG_PC_BITS	4
#define TAG_DATA_BITS	4

#define TAG_PERM_READ	(1 << 0)
#define TAG_PERM_WRITE	(1 << 1)
#define TAG_PERM_EXEC	(1 << 2)

#define TSR_EG		(1 << 27)	/* Exception Globals */
#define TSR_PEG		(1 << 26)	/* Previous Exception Globals */
#define TSR_T		(1 << 25)	/* Trust */
#define TSR_PT		(1 << 24)	/* Previous Trust */

#ifndef __ASSEMBLER__
#include <machine/types.h>

void	tag_init(void);
void	tag_trap(void) __attribute__((noreturn));
void	tag_set(const void *addr, uint32_t dtag, size_t n);
#endif

#endif
