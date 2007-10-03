#include <asm-generic/vmlinux.lds.h>

OUTPUT_FORMAT(ELF_FORMAT)
OUTPUT_ARCH(ELF_ARCH)

ENTRY(_start)
jiffies = jiffies_64;

SECTIONS
{
  PROVIDE (__executable_start = 0x400000);
  . = 0x400000 + SIZEOF_HEADERS;

  _stext = .;

  .interp         : { *(.interp) }
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rel.dyn        :
    { 
      *(.rel.init)
      *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
      *(.rel.fini)
      *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
      *(.rel.data.rel.ro* .rel.gnu.linkonce.d.rel.ro.*)
      *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
      *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
      *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
      *(.rel.ctors)
      *(.rel.dtors)
      *(.rel.got)
      *(.rel.sharable_data .rel.sharable_data.* .rel.gnu.linkonce.shrd.*)
      *(.rel.sharable_bss .rel.sharable_bss.* .rel.gnu.linkonce.shrb.*)
      *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
      *(.rel.ldata .rel.ldata.* .rel.gnu.linkonce.l.*)
      *(.rel.lbss .rel.lbss.* .rel.gnu.linkonce.lb.*)
      *(.rel.lrodata .rel.lrodata.* .rel.gnu.linkonce.lr.*)
    }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.sharable_data .rela.sharable_data.* .rela.gnu.linkonce.shrd.*)
      *(.rela.sharable_bss .rela.sharable_bss.* .rela.gnu.linkonce.shrb.*)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      *(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*)
      *(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*)
      *(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*)
    }
  .rel.plt        : { *(.rel.plt) }
  .rela.plt       : { *(.rela.plt) }
  .plt            : { *(.plt) }

  .text : {
    _stext = .;
    _text = .;
    *(.text)
    SCHED_TEXT
    LOCK_TEXT
    *(.fixup)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    *(.gnu.linkonce.t*)
  }

  .init.text : {
    _sinittext = .;
    *(.init.text)
    _einittext = .;
  }

  .fini : {
    *(.fini)
  }

  _etext = .;

  . = ALIGN(4096);

  _sdata = .;

  .eh_frame_hdr   : { *(.eh_frame_hdr) }
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) }
  .gcc_except_table : ONLY_IF_RO { *(.gcc_except_table .gcc_except_table.*) }

  . = ALIGN(4096);

  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) }
  .gcc_except_table : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }

  .data : {
    . = ALIGN(KERNEL_STACK_SIZE);		/* init_task */
    *(.data.init_task)
    *(.data)
    *(.gnu.linkonce.d*)
    CONSTRUCTORS
  }

  .init.data : {
    *(init.data)
  }

  #include "asm/common.lds.S"

  .data1   : { *(.data1) }
  .ctors         :
  {
    *(.ctors)
  }
  .dtors         :
  {
    *(.dtors)
  }

  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro* .gnu.linkonce.d.rel.ro.*) }
  .dynamic        : { *(.dynamic) }
  .got            : { *(.got) }
  .got.plt        : { *(.got.plt) }

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
