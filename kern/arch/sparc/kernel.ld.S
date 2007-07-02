#include <machine/memlayout.h>
	
OUTPUT_FORMAT("elf32-sparc", "elf32-sparc", "elf32-sparc")
OUTPUT_ARCH("sparc")
ENTRY(prom_start)

SECTIONS
{
	. = KERNBASE;
  
	PROVIDE(stext = .);	
	.text : AT(ADDR(.text) - LOAD_OFFSET){
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}

	PROVIDE(etext = .);

	PROVIDE(srodata = .);
	.rodata : AT(ADDR(.rodata) - LOAD_OFFSET) {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}
	PROVIDE(erodata = .);

	. = ALIGN(0x1000);

	PROVIDE(sdata = .);
	.data : AT(ADDR(.data) - LOAD_OFFSET) {
		*(.data)
	}
	PROVIDE(edata = .);

	PROVIDE(sbss = .);	
	.bss : AT(ADDR(.bss) - LOAD_OFFSET) {
		*(.bss)
	}
	PROVIDE(ebss = .);

	/* layout for the kernel stack */
	. += 0x1000;
	. = ALIGN(0x1000);

	PROVIDE(kstack_bottom = .);
	. += 0x4000;
	PROVIDE(kstack_top = .);	

	PROVIDE(end = .);

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}
}
