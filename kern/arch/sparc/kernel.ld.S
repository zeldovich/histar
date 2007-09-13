#include <machine/memlayout.h>
#include <machine/pmap.h>
		
OUTPUT_FORMAT("elf32-sparc", "elf32-sparc", "elf32-sparc")
OUTPUT_ARCH("sparc")

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
	PROVIDE(kstack = .);
	. += KSTACK_SIZE;
	PROVIDE(kstack_top = .);

	/* monitor-mode stack */
	PROVIDE(monstack = .);
	. += KSTACK_SIZE;
	PROVIDE(monstack_top = .);

	PROVIDE(end = .);

	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}
}
