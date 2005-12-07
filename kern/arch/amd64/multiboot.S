/*
 * This file must be the first one in the link order, 
 * so that mboothdr is the start of the text segment.
 */

#include <machine/memlayout.h>

#define	MULTIBOOT_PAGE_ALIGN	(1<<0)
#define MULTIBOOT_MEMORY_INFO	(1<<1)
#define MULTIBOOT_AOUT_KLUDGE	(1<<16)
#define MULTIBOOT_HEADER_MAGIC	(0x1BADB002)
#define MULTIBOOT_HEADER_FLAGS	(MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE)
#define CHECKSUM		(-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS))

.text
.align 4
.code32
mboothdr:

.long	MULTIBOOT_HEADER_MAGIC
.long	MULTIBOOT_HEADER_FLAGS
.long	CHECKSUM

.long	RELOC(mboothdr)	// Address of the multiboot header
.long	RELOC(mboothdr)	// Start of the text segment
.long	RELOC(edata)	// End of the data segment
.long	RELOC(end)	// End of BSS segment
.long	RELOC(start)	// Entrypoint

