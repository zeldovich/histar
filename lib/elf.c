#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>

int
elf_load(uint64_t container, struct cobj_ref seg, struct thread_entry *e)
{
    char elfname[KOBJ_NAME_LEN], objname[KOBJ_NAME_LEN];
    int r = sys_obj_get_name(seg, &elfname[0]);
    if (r < 0)
	return r;

    char *segbuf = 0;
    uint64_t bytes;
    r = segment_map(seg, SEGMAP_READ, (void**)&segbuf, &bytes);
    if (r < 0) {
	cprintf("elf_load: cannot map segment\n");
	return r;
    }

    struct u_segment_mapping sm_ents[16];
    int si = 0;

    Elf64_Ehdr *elf = (Elf64_Ehdr*) segbuf;
    if (elf->e_magic != ELF_MAGIC || elf->e_ident[0] != 2) {
	cprintf("elf_load: ELF magic mismatch\n");
	return -E_INVAL;
    }

    e->te_entry = (void*) elf->e_entry;
    Elf64_Phdr *ph = (Elf64_Phdr *) (segbuf + elf->e_phoff);
    for (int i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != 1)
	    continue;

	int va_off = ph->p_vaddr & 0xfff;
	struct cobj_ref nseg;
	char *sbuf = 0;
	r = segment_alloc(container, va_off + ph->p_memsz,
			  &nseg, (void**) &sbuf);
	if (r < 0) {
	    cprintf("elf_load: cannot allocate elf segment: %s\n", e2s(r));
	    return r;
	}

	snprintf(&objname[0], KOBJ_NAME_LEN, "text/data for %s", elfname);
	r = sys_obj_set_name(nseg, &objname[0]);
	if (r < 0)
	    return r;

	memcpy(sbuf + va_off, segbuf + ph->p_offset, ph->p_filesz);
	r = segment_unmap(sbuf);
	if (r < 0) {
	    cprintf("elf_load: cannot unmap elf segment: %s\n", e2s(r));
	    return r;
	}

	sm_ents[si].segment = nseg;
	sm_ents[si].start_page = 0;
	sm_ents[si].num_pages = (va_off + ph->p_memsz + PGSIZE - 1) / PGSIZE;
	sm_ents[si].flags = ph->p_flags;
	sm_ents[si].va = (void*) (ph->p_vaddr - va_off);
	si++;
    }

    r = segment_unmap(segbuf);
    if (r < 0) {
	cprintf("elf_load: cannot unmap program segment: %s\n", e2s(r));
	return r;
    }

    int stackpages = 2;
    struct cobj_ref stack;
    r = segment_alloc(container, stackpages * PGSIZE, &stack, 0);
    if (r < 0) {
	cprintf("elf_load: cannot create stack segment: %s\n", e2s(r));
	return r;
    }

    snprintf(&objname[0], KOBJ_NAME_LEN, "stack for %s", elfname);
    r = sys_obj_set_name(stack, &objname[0]);
    if (r < 0)
	return r;

    char *stacktop = (char*) USTACKTOP;
    e->te_stack = stacktop;

    sm_ents[si].segment = stack;
    sm_ents[si].start_page = 0;
    sm_ents[si].num_pages = stackpages;
    sm_ents[si].flags = SEGMAP_READ | SEGMAP_WRITE;
    sm_ents[si].va = stacktop - stackpages * PGSIZE;
    si++;

    struct u_address_space uas = { .nent = si, .ents = &sm_ents[0] };
    int64_t as_id = sys_as_create(container);
    if (as_id < 0) {
	cprintf("elf_load: cannot create address space: %s\n", e2s(r));
	return as_id;
    }

    e->te_as = COBJ(container, as_id);
    r = sys_obj_set_name(e->te_as, &elfname[0]);
    if (r < 0)
	return r;

    r = sys_as_set(e->te_as, &uas);
    if (r < 0) {
	cprintf("elf_load: cannot load address space: %s\n", e2s(r));
	return r;
    }

    return 0;
}
