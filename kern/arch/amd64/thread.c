#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <inc/elf64.h>

struct Thread *cur_thread;
struct Thread_list thread_list;

static void
map_segment(uint64_t *pgmap, void *va, size_t len)
{
    void *endva = (char*) va + len;

    while (va < endva) {
	struct Page *pp;
	int r = page_alloc(&pp);
	if (r < 0)
	    panic("map_segment: cannot alloc page");

	r = page_insert(pgmap, pp, va, PTE_U | PTE_W);
	if (r < 0) {
	    page_free(pp);
	    panic("map_segment: cannot insert page");
	}

	va = ROUNDDOWN((char*) va + PGSIZE, PGSIZE);
    }
}

static void
load_icode(struct Thread *t, uint8_t *binary, size_t size)
{
    // Switch to target address space to populate it
    lcr3(t->cr3);

    // Map a stack
    map_segment(t->pgmap, (void*) (ULIM - PGSIZE), PGSIZE);

    Elf64_Ehdr *elf = (Elf64_Ehdr *) binary;
    if (elf->e_magic != ELF_MAGIC || elf->e_ident[0] != 2)
	panic("ELF magic mismatch");

    int i;
    Elf64_Phdr *ph = (Elf64_Phdr *) (binary + elf->e_phoff);
    for (i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != 1)
	    continue;
	if (ph->p_vaddr + ph->p_memsz < ph->p_vaddr)
	    panic("elf segment overflow");
	if (ph->p_vaddr + ph->p_memsz > ULIM)
	    panic("elf segment over ULIM");

	map_segment(t->pgmap, (void*) ph->p_vaddr, ph->p_memsz);
    }

    // Two passes so that map_segment() doesn't drop a partially-filled
    // page from a previous ELF segment.
    ph = (Elf64_Phdr *) (binary + elf->e_phoff);
    for (i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != 1)
	    continue;

	memcpy((void*) ph->p_vaddr, binary + ph->p_offset, ph->p_filesz);
	memset((void*) ph->p_vaddr + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    memset(&t->tf, 0, sizeof(t->tf));
    t->tf.tf_ss = GD_UD | 3;
    t->tf.tf_rsp = ULIM;
    t->tf.tf_rflags = FL_IF;
    t->tf.tf_cs = GD_UT | 3;
    t->tf.tf_rip = elf->e_entry;
}

void
thread_create_first(struct Thread *t, uint8_t *binary, size_t size)
{
    struct Page *pgmap_p;
    int r = page_alloc(&pgmap_p);
    if (r < 0)
	panic("thread_create_first: cannot alloc pml4");
    pgmap_p->pp_ref++;

    t->cr3 = page2pa(pgmap_p);
    t->pgmap = (uint64_t *) page2kva(pgmap_p);
    memcpy(t->pgmap, bootpml4, PGSIZE);

    load_icode(t, binary, size);

    t->status = thread_runnable;
    LIST_INSERT_HEAD(&thread_list, t, link);
}

void
thread_run(struct Thread *t)
{
    cur_thread = t;
    lcr3(t->cr3);
    trapframe_pop(&t->tf);
}

void
thread_kill(struct Thread *t)
{
    LIST_REMOVE(t, link);
    // XXX
    // garbage collection, eventually
}
