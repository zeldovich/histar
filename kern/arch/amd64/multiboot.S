/*
 * This file must be the first one in the link order, 
 * so that mboothdr is the start of the text segment.
 */

#include <machine/memlayout.h>
#include <machine/multiboot.h>

#define HEADER_FLAGS	(MULTIBOOT_HEADER_PAGE_ALIGN | \
			 MULTIBOOT_HEADER_MEMORY_INFO | \
			 MULTIBOOT_HEADER_AOUT_KLUDGE)
#define CHECKSUM	(-(MULTIBOOT_HEADER_MAGIC + HEADER_FLAGS))

.text
.align 4
.code32
mboothdr:

.long	MULTIBOOT_HEADER_MAGIC
.long	HEADER_FLAGS
.long	CHECKSUM

.long	RELOC(mboothdr)	// header_addr: Address of the multiboot header
.long	RELOC(mboothdr)	// load_addr: Start of the text segment
.long	RELOC(edata)	// load_end_addr: End of the data segment
.long	RELOC(end)	// bss_end_addr: End of BSS segment
.long	RELOC(start)	// entry_addr: Entrypoint

