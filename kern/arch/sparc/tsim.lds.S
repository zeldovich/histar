#include <machine/bootloader.h>

OUTPUT_FORMAT("elf32-sparc", "elf32-sparc", "elf32-sparc")
OUTPUT_ARCH("sparc")
ENTRY(_start)
SECTIONS
{
	.rom 0 : {
		_start = .;
		*(.prom_stage1.text)
	}

	. = BOOTLOADER_physbase /*0x40000000*/;

	/* XXX prom_stage2?  Do what linux does? */
	
	/* everything else for now ...*/
}
