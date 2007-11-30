/* vi: set sw=4 ts=4: */
/*
 * Architecture specific code used by dl-startup.c
 * Copyright (C) 2000-2004 by Erik Andersen <andersen@codepoet.org>
 */
__asm__ (
    "	.text\n"
    "	.align 16\n"
    "	.globl	_start\n"
    "	.type	_start,@function\n"
    "_start:\n"
	"   # The auxvt is on the stack\n"
	"   pushl %esp\n"
	"   pushl %ecx\n"
	"   pushl %edx\n"
	"   pushl %eax\n"
    "	call _dl_start\n"
    "	# Save the user entry point address in %edi.\n"
    "	movl %eax, %edi\n"
	"   # Restore arguments for libmain and friends.\n"
	"   popl %eax\n"
	"   popl %edx\n"
	"   popl %ecx\n"
	"   # Pass magic value 2 as arg0, to avoid setup_env.\n"
	"   movl $2, %eax\n"
    "	# Point %ebx at the GOT.\n"
    "	call 1f\n"
    "1:	popl	%ebx\n"
    "	addl $_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx\n"
#if 0
    "	# See if we were run as a command with the executable file\n"
    "	# name as an extra leading argument.\n"
    "	movl _dl_skip_args@GOTOFF(%ebx), %eax\n"
    "	# Pop the original argument count.\n"
    "	popl %edx\n"
    "	# Adjust the stack pointer to skip _dl_skip_args words.\n"
    "	leal (%esp,%eax,4), %esp\n"
    "	# Subtract _dl_skip_args from argc.\n"
    "	subl %eax, %edx\n"
    "	# Push argc back on the stack.\n"
    "	push %edx\n"
    "	# Pass our FINI ptr() to the user in %edx, as per ELF ABI.\n"
    "	leal _dl_fini@GOTOFF(%ebx), %edx\n"
#endif
    "	# Jump to the user's entry point.\n"
    "	jmp *%edi\n"
    "	.size	_start,.-_start\n"
    "	.previous\n"
);

/* Handle relocation of the symbols in the dynamic loader. */
static __always_inline
void PERFORM_BOOTSTRAP_RELOC(ELF_RELOC *rpnt, unsigned long *reloc_addr,
	unsigned long symbol_addr, unsigned long load_addr, attribute_unused Elf32_Sym *symtab)
{
	switch (ELF32_R_TYPE(rpnt->r_info))
	{
		case R_386_32:
			*reloc_addr += symbol_addr;
			break;
		case R_386_PC32:
			*reloc_addr += symbol_addr - (unsigned long) reloc_addr;
			break;
		case R_386_GLOB_DAT:
		case R_386_JMP_SLOT:
			*reloc_addr = symbol_addr;
			break;
		case R_386_RELATIVE:
			*reloc_addr += load_addr;
			break;
		default:
			_dl_exit(1);
	}
}
