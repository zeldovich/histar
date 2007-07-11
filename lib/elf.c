#include <machine/param.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/elf32.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <stdio.h>
#include <string.h>

#if JOS_ARCH_BITS==32
#define ARCH_ELF_EHDR	Elf32_Ehdr
#define ARCH_ELF_PHDR	Elf32_Phdr
#define ARCH_ELF_CLASS	ELF_CLASS_32
#elif JOS_ARCH_BITS==64
#define ARCH_ELF_EHDR	Elf64_Ehdr
#define ARCH_ELF_PHDR	Elf64_Phdr
#define ARCH_ELF_CLASS	ELF_CLASS_64
#else
#error What is this architecture?
#endif

#if JOS_ARCH_ENDIAN==JOS_LITTLE_ENDIAN
#define ARCH_ELF_MAGIC	ELF_MAGIC_LE
#elif JOS_ARCH_ENDIAN==JOS_BIG_ENDIAN
#define ARCH_ELF_MAGIC	ELF_MAGIC_BE
#else
#error What is this architecture?
#endif

enum { elf_debug = 0 };

int
elf_load(uint64_t container, struct cobj_ref seg, struct thread_entry *e,
	 struct ulabel *label)
{
    char elfname[KOBJ_NAME_LEN], objname[KOBJ_NAME_LEN];
    int r = sys_obj_get_name(seg, &elfname[0]);
    if (r < 0)
	return r;

    uint64_t seglen = 0;
    void *segbuf = 0;
    r = segment_map(seg, 0, SEGMAP_READ, (void**)&segbuf, &seglen, 0);
    if (r < 0) {
	cprintf("elf_load: cannot map segment\n");
	return r;
    }

    struct u_segment_mapping sm_ents[8];
    int si = 0;

    ARCH_ELF_EHDR *elf = (ARCH_ELF_EHDR*) segbuf;
    if (elf->e_magic != ARCH_ELF_MAGIC) {
	if (elf_debug)
	    cprintf("elf_load: ELF magic mismatch\n");
	return -E_INVAL;
    }

    if (elf->e_type != ELF_TYPE_EXEC) {
	cprintf("elf_load: ELF file not an executable\n");
	return -E_INVAL;
    }

    if (elf->e_ident[EI_CLASS] != ARCH_ELF_CLASS) {
	cprintf("elf_load: ELF file wrong class\n");
	return -E_INVAL;
    }

    int64_t copy_id = sys_segment_copy(seg, container, 0,
				       "text/data segment copy");
    if (copy_id < 0)
	return copy_id;
    struct cobj_ref copyseg = COBJ(container, copy_id);

    int stackpages = 2;
    int shared_stack = 0;
    struct cobj_ref stack;
    uint64_t stack_pgoff = 0;

    e->te_entry = (void*) elf->e_entry;
    ARCH_ELF_PHDR *ph = (ARCH_ELF_PHDR *) (segbuf + elf->e_phoff);
    for (int i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != ELF_PROG_LOAD)
	    continue;

	int va_off = ph->p_vaddr & 0xfff;

	if (ph->p_memsz <= ph->p_filesz) {
	    sm_ents[si].segment = copyseg;
	    sm_ents[si].start_page = (ph->p_offset - va_off) / PGSIZE;
	    sm_ents[si].num_pages = (va_off + ph->p_memsz + PGSIZE - 1) / PGSIZE;
	    sm_ents[si].flags = ph->p_flags;
	    sm_ents[si].va = (void*) (ph->p_vaddr - va_off);
	    si++;
	} else {
	    uintptr_t shared_pages = (va_off + ph->p_filesz) / PGSIZE;

	    sm_ents[si].segment = copyseg;
            sm_ents[si].start_page = (ph->p_offset - va_off) / PGSIZE;
            sm_ents[si].num_pages = shared_pages;
            sm_ents[si].flags = ph->p_flags;
            sm_ents[si].va = (void*) (ph->p_vaddr - va_off);
	    si++;

	    struct cobj_ref nseg;
	    char *sbuf = 0;
	    snprintf(&objname[0], KOBJ_NAME_LEN, "text/data for %s", elfname);
	    r = segment_alloc(container,
			      va_off + ph->p_memsz + (stackpages + 1 - shared_pages) * PGSIZE,
			      &nseg, (void**) &sbuf, label, &objname[0]);
	    if (r < 0) {
		cprintf("elf_load: cannot allocate elf segment: %s\n", e2s(r));
		return r;
	    }

	    memcpy(sbuf, segbuf + ph->p_offset - va_off + shared_pages * PGSIZE,
		   va_off + ph->p_filesz - shared_pages * PGSIZE);
	    r = segment_unmap(sbuf);
	    if (r < 0) {
		cprintf("elf_load: cannot unmap elf segment: %s\n", e2s(r));
		return r;
	    }

	    sm_ents[si].segment = nseg;
	    sm_ents[si].start_page = 0;
	    sm_ents[si].num_pages = (va_off + ph->p_memsz + PGSIZE - 1) / PGSIZE - shared_pages;
	    sm_ents[si].flags = ph->p_flags;
	    sm_ents[si].va = (void*) (ph->p_vaddr - va_off + shared_pages * PGSIZE);
	    si++;

	    stack = nseg;
	    stack_pgoff = (va_off + ph->p_memsz + PGSIZE - 1) / PGSIZE - shared_pages;
	    shared_stack = 1;
	}
    }

    r = segment_unmap(segbuf);
    if (r < 0) {
	cprintf("elf_load: cannot unmap program segment: %s\n", e2s(r));
	return r;
    }

    if (!shared_stack) {
	snprintf(&objname[0], KOBJ_NAME_LEN, "stack for %s", elfname);
	r = segment_alloc(container, stackpages * PGSIZE, &stack, 0, label, &objname[0]);
	if (r < 0) {
	    cprintf("elf_load: cannot create stack segment: %s\n", e2s(r));
	    return r;
	}

	stack_pgoff = 0;
    }

    char *stacktop = (char*) USTACKTOP;
    e->te_stack = stacktop;

    sm_ents[si].segment = stack;
    sm_ents[si].start_page = stack_pgoff;
    sm_ents[si].num_pages = thread_stack_pages;
    sm_ents[si].flags = SEGMAP_READ | SEGMAP_WRITE | SEGMAP_STACK |
			SEGMAP_REVERSE_PAGES;
    sm_ents[si].va = stacktop - thread_stack_pages * PGSIZE;
    si++;

    struct u_address_space uas = { .trap_handler = 0,
				   .trap_stack_base = 0,
				   .trap_stack_top = 0,
				   .nent = si, .ents = &sm_ents[0] };
    int64_t as_id = sys_as_create(container, label, &elfname[0]);
    if (as_id < 0) {
	cprintf("elf_load: cannot create address space: %s\n", e2s(as_id));
	return as_id;
    }

    e->te_as = COBJ(container, as_id);
    r = sys_as_set(e->te_as, &uas);
    if (r < 0) {
	cprintf("elf_load: cannot load address space: %s\n", e2s(r));
	return r;
    }

    return 0;
}
