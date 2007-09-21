#include <asm-generic/vmlinux.lds.h>

  RODATA

  .unprotected : { *(.unprotected) }
  . = ALIGN(4096);
  PROVIDE (_unprotected_end = .);

  . = ALIGN(4096);
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __setup_start = .;
  .init.setup : { *(.init.setup) }
  __setup_end = .;

  . = ALIGN(32);
  __per_cpu_start = . ; 
  .data.percpu : { *(.data.percpu) }
  __per_cpu_end = . ;
	
  __initcall_start = .;
  .initcall.init : {
	INITCALLS
  }
  __initcall_end = .;

  __con_initcall_start = .;
  .con_initcall.init : { *(.con_initcall.init) }
  __con_initcall_end = .;

  __init_end = .;

  SECURITY_INIT

  __exitcall_begin = .;
  .exitcall : { *(.exitcall.exit) }
  __exitcall_end = .;

  __preinit_array_start = .;
  .preinit_array : { *(.preinit_array) }
  __preinit_array_end = .;
  __init_array_start = .;
  .init_array : { *(.init_array) }
  __init_array_end = .;
  __fini_array_start = .;
  .fini_array : { *(.fini_array) }
  __fini_array_end = .;

   . = ALIGN(4096);
  __initramfs_start = .;
  .init.ramfs : { *(.init.ramfs) }
  __initramfs_end = .;

  /* Sections to be discarded */
  /DISCARD/ : {
 	*(.exitcall.exit)
  }

