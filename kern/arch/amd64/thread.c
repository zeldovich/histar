#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <inc/elf64.h>
#include <inc/error.h>

struct Thread *cur_thread;
struct Thread_list thread_list;

static int
map_segment(struct Pagemap *pgmap, void *va, size_t len)
{
    void *endva = (char*) va + len;

    while (va < endva) {
	struct Page *pp;
	int r = page_alloc(&pp);
	if (r < 0)
	    return r;

	r = page_insert(pgmap, pp, va, PTE_U | PTE_W);
	if (r < 0) {
	    page_free(pp);
	    return r;
	}

	va = ROUNDDOWN((char*) va + PGSIZE, PGSIZE);
    }

    return 0;
}

int
thread_load_elf(struct Thread *t, uint8_t *binary, size_t size)
{
    // Switch to target address space to populate it
    lcr3(t->th_cr3);

    // Map a stack
    map_segment(t->th_pgmap, (void*) (ULIM - PGSIZE), PGSIZE);

    Elf64_Ehdr *elf = (Elf64_Ehdr *) binary;
    if (elf->e_magic != ELF_MAGIC || elf->e_ident[0] != 2) {
	cprintf("ELF magic mismatch\n");
	return -E_INVAL;
    }

    int i;
    Elf64_Phdr *ph = (Elf64_Phdr *) (binary + elf->e_phoff);
    for (i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != 1)
	    continue;
	if (ph->p_vaddr + ph->p_memsz < ph->p_vaddr) {
	    cprintf("ELF segment overflow\n");
	    return -E_INVAL;
	}
	if (ph->p_vaddr + ph->p_memsz > ULIM) {
	    cprintf("ELF segment over ULIM\n");
	    return -E_INVAL;
	}

	int r = map_segment(t->th_pgmap, (void*) ph->p_vaddr, ph->p_memsz);
	if (r < 0)
	    return r;
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

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rsp = ULIM;
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT | 3;
    t->th_tf.tf_rip = elf->e_entry;

    return 0;
}

void
thread_set_runnable(struct Thread *t)
{
    t->th_status = thread_runnable;
}

int
thread_alloc(struct Thread **tp)
{
    struct Page *thread_pg;
    int r = page_alloc(&thread_pg);
    if (r < 0)
	return r;

    struct Thread *t = page2kva(thread_pg);

    memset(t, 0, sizeof(*t));
    LIST_INSERT_HEAD(&thread_list, t, th_link);
    t->th_status = thread_not_runnable;

    struct Page *pgmap_p;
    r = page_alloc(&pgmap_p);
    if (r < 0) {
	thread_free(t);
	return r;
    }

    pgmap_p->pp_ref++;
    t->th_cr3 = page2pa(pgmap_p);
    t->th_pgmap = page2kva(pgmap_p);
    memcpy(t->th_pgmap, bootpml4, PGSIZE);

    *tp = t;
    return 0;
}

void
thread_free(struct Thread *t)
{
    LIST_REMOVE(t, th_link);
    if (t->th_pgmap)
	page_map_decref(t->th_pgmap);

    struct Page *thread_pg = page_lookup_cur(t);
    if (thread_pg == 0)
	panic("thread_free cannot find thread page");

    page_free(thread_pg);
}

void
thread_run(struct Thread *t)
{
    cur_thread = t;
    lcr3(t->th_cr3);
    trapframe_pop(&t->th_tf);
}

void
thread_kill(struct Thread *t)
{
    t->th_status = thread_not_runnable;
    if (cur_thread == t)
	cur_thread = 0;
}
