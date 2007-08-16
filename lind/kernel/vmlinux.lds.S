#include <asm-generic/vmlinux.lds.h>

OUTPUT_FORMAT(ELF_FORMAT)
OUTPUT_ARCH(ELF_ARCH)

ENTRY(_start)
jiffies = jiffies_64;

PHDRS {
	text PT_LOAD FLAGS(5);	/* R_X */
	data PT_LOAD FLAGS(7);	/* RWX */
	note PT_NOTE FLAGS(4);	/* R__ */
}

SECTIONS
{
  PROVIDE (__executable_start = 0x400000);
  . = 0x400000 + SIZEOF_HEADERS;

  _stext = .;

  .text : {
    _text = .;
    *(.text)
    SCHED_TEXT
    LOCK_TEXT
    *(.fixup)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    *(.gnu.linkonce.t*)
  } :text

  .data : {
    . = ALIGN(KERNEL_STACK_SIZE);		/* init_task */
    *(.data.init_task)
    *(.data)
    *(.gnu.linkonce.d*)
    CONSTRUCTORS
  } :data

  #include "asm/common.lds.S"

  . = ALIGN(4096);
  .init.text : {
    _sinittext = .;
    *(.init.text)
    _einittext = .;
  }
  . = ALIGN(4096);

  .init.data : { *(init.data) }

  .data1   : { *(.data1) }
  .ctors         :
  {
    *(.ctors)
  }
  .dtors         :
  {
    *(.dtors)
  }

  .got           : { *(.got.plt) *(.got) }
  .dynamic       : { *(.dynamic) }
  .tdata	  : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss		  : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata     : { *(.sdata) }
  _edata  =  .;
  PROVIDE (edata = .);
  . = ALIGN(0x1000);
  .sbss      :
  {
   __bss_start = .;
   PROVIDE(_bss_start = .);
   *(.sbss)
   *(.scommon)
  }
  .bss       :
  {
   *(.dynbss)
   *(.bss)
   *(COMMON)
  }
  _end = .;
  PROVIDE (end = .);

  STABS_DEBUG

  DWARF_DEBUG
}
