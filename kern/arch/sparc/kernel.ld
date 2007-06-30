OUTPUT_FORMAT("elf32-sparc", "elf32-sparc", "elf32-sparc")
OUTPUT_ARCH(sparc)
ENTRY(prom_start)

SECTIONS
{
	. = 0x40000000;

	PROVIDE(stext = .);	
	.text : {
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}
	PROVIDE(etext = .);

	PROVIDE(srodata = .);
	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}
	PROVIDE(erodata = .);

	. = ALIGN(0x1000);

	PROVIDE(sdata = .);
	.data : {
		*(.data)
	}
	PROVIDE(edata = .);


	PROVIDE(sbss = .);	
	.bss : {
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
