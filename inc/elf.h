#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H

#define ELF_MAGIC_BE		0x7F454C46	/* "\x7FELF" in big endian */
#define ELF_MAGIC_LE		0x464C457F	/* "\x7FELF" in little endian */

// e_type values
#define ELF_TYPE_EXEC		2
#define ELF_TYPE_DYNAMIC	3
#define ELF_TYPE_CORE		4

// e_ident indexes
#define EI_CLASS		0
#define EI_DATA			1
#define EI_VERSION		2
#define EI_ABI			3

// e_ident[EI_CLASS]
#define ELF_CLASS_32		1
#define ELF_CLASS_64		2

// e_machine values
#define ELF_MACH_SPARC		2
#define ELF_MACH_386		3
#define ELF_MACH_486		6
#define ELF_MACH_AMD64		62

// p_type values
#define ELF_PROG_LOAD		1
#define ELF_PROG_INTERP		3

// p_flags
#define ELF_PF_X		0x01
#define ELF_PF_W		0x02
#define ELF_PF_R		0x04

// sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// sh_name
#define ELF_SHN_UNDEF		0

// Symbol table index
#define ELF_STN_UNDEF		0

// macros for st_info
#define ELF_ST_BIND(i)		((i) >> 4)
#define ELF_ST_TYPE(i)		((i) & 0xF)
#define ELF_ST_INFO(b, t)	((b) << 4 | ((t) & 0xF))

// auxiliary table entry types
#define ELF_AT_NULL		0		/* End of vector */
#define ELF_AT_IGNORE		1		/* Entry should be ignored */
#define ELF_AT_EXECFD		2		/* File descriptor of program */
#define ELF_AT_PHDR		3		/* Program headers for program */
#define ELF_AT_PHENT		4		/* Size of program header entry */
#define ELF_AT_PHNUM		5		/* Number of program headers */
#define ELF_AT_PAGESZ		6		/* System page size */
#define ELF_AT_BASE		7		/* Base address of interpreter */
#define ELF_AT_FLAGS		8		/* Flags */
#define ELF_AT_ENTRY		9		/* Entry point of program */
#define ELF_AT_NOTELF		10		/* Program is not ELF */
#define ELF_AT_UID		11		/* Real uid */
#define ELF_AT_EUID		12		/* Effective uid */
#define ELF_AT_GID		13		/* Real gid */
#define ELF_AT_EGID		14		/* Effective gid */
#define ELF_AT_CLKTCK		17		/* Frequency of times() */

#endif
