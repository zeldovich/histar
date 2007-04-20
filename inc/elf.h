#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H

#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */

// e_type values
#define ELF_TYPE_EXEC		2
#define ELF_TYPE_CORE		4

// e_machine values
#define ELF_MACH_386		3
#define ELF_MACH_486		6
#define ELF_MACH_AMD64		62

// p_type values
#define ELF_PROG_LOAD		1

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

#endif
