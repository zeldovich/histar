#ifndef JOS_INC_ELF64_H
#define JOS_INC_ELF64_H

#include <inc/types.h>
#include <inc/elf.h>

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef  int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef  int64_t Elf64_Sxword;
typedef  int16_t Elf64_Section;

typedef struct {
  Elf64_Word      e_magic;
  unsigned char   e_ident[12];
  Elf64_Half      e_type;
  Elf64_Half      e_machine;
  Elf64_Word      e_version;
  Elf64_Addr      e_entry;
  Elf64_Off       e_phoff;
  Elf64_Off       e_shoff;
  Elf64_Word      e_flags;
  Elf64_Half      e_ehsize;
  Elf64_Half      e_phentsize;
  Elf64_Half      e_phnum;
  Elf64_Half      e_shentsize;
  Elf64_Half      e_shnum;
  Elf64_Half      e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  Elf64_Word	p_type;
  Elf64_Word	p_flags;
  Elf64_Off	p_offset;
  Elf64_Addr	p_vaddr;
  Elf64_Addr	p_paddr;
  Elf64_Xword	p_filesz;
  Elf64_Xword	p_memsz;
  Elf64_Xword	p_align;
} Elf64_Phdr;

typedef struct {
  Elf64_Word	sh_name;
  Elf64_Word	sh_type;
  Elf64_Xword	sh_flags;
  Elf64_Addr	sh_addr;
  Elf64_Off	sh_offset;
  Elf64_Xword	sh_size;
  Elf64_Word	sh_link;
  Elf64_Word	sh_info;
  Elf64_Xword	sh_addralign;
  Elf64_Xword	sh_entsize;
} Elf64_Shdr;

typedef struct {
  Elf64_Word	st_name;
  unsigned char	st_info;
  unsigned char	st_other;
  Elf64_Half	st_shndx;
  Elf64_Addr	st_value;
  Elf64_Xword	st_size;
} Elf64_Sym;

#endif /* !JOS_INC_ELF_H */
