#ifndef JOS_MACHINE_ASI_H
#define JOS_MACHINE_ASI_H

#include <machine/leon.h>

/* Address Space Identifier values for LEON3 sparc */

#define ASI_MMUREGS	ASI_LEON_MMUREGS
#define ASI_BYPASS	ASI_LEON_BYPASS
#define ASI_MMUFLUSH	ASI_LEON_MMUFLUSH
#define ASI_DFLUSH	ASI_LEON_DFLUSH

#define	ASI_USER_TEXT	0x08
#define ASI_SUPER_TEXT	0x09
#define ASI_USER_DATA	0x0A
#define ASI_SUPER_DATA	0x0B

#endif
