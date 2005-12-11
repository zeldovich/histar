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
    char *segbuf;
    uint64_t bytes;
    int r = segment_map(container, seg, 0, (void**)&segbuf, &bytes);
    if (r < 0) {
	cprintf("elf_load: cannot map segment\n");
	return r;
    }

    memset(e, 0, sizeof(*e));
    struct segment_map *segmap = &e->te_segmap;
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
	struct cobj_ref seg;
	r = segment_alloc(container, va_off + ph->p_memsz, &seg);
	if (r < 0) {
	    cprintf("elf_load: cannot allocate elf segment: %s\n", e2s(r));
	    return r;
	}

	char *sbuf;
	r = segment_map(container, seg, 1, (void**)&sbuf, 0);
	if (r < 0) {
	    cprintf("elf_load: cannot map elf segment: %s\n", e2s(r));
	    return r;
	}

	memcpy(sbuf + va_off, segbuf + ph->p_offset, ph->p_filesz);
	r = segment_unmap(container, sbuf);
	if (r < 0) {
	    cprintf("elf_load: cannot unmap elf segment: %s\n", e2s(r));
	    return r;
	}

	segmap->sm_ent[si].segment = seg;
	segmap->sm_ent[si].start_page = 0;
	segmap->sm_ent[si].num_pages = (va_off + ph->p_memsz +
					PGSIZE - 1) / PGSIZE;
	segmap->sm_ent[si].writable = (ph->p_flags & ELF_PF_W) ? 1 : 0;
	segmap->sm_ent[si].va = (void*) (ph->p_vaddr - va_off);
	si++;
    }

    r = segment_unmap(container, segbuf);
    if (r < 0) {
	cprintf("elf_load: cannot unmap program segment: %s\n", e2s(r));
	return r;
    }

    int stackpages = 2;
    struct cobj_ref stack;
    r = segment_alloc(container, stackpages * PGSIZE, &stack);
    if (r < 0) {
	cprintf("elf_load: cannot create stack segment: %s\n", e2s(r));
	return r;
    }

    char *stacktop = (char*) ULIM;
    e->te_stack = stacktop;
    segmap->sm_ent[si].segment = stack;
    segmap->sm_ent[si].start_page = 0;
    segmap->sm_ent[si].num_pages = stackpages;
    segmap->sm_ent[si].writable = 1;
    segmap->sm_ent[si].va = stacktop - stackpages * PGSIZE;

    return 0;
}
