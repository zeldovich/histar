#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>

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

    map_segment(t->pgmap, (void*) 0xff0000, PGSIZE);
    map_segment(t->pgmap, (void*) (ULIM - PGSIZE), PGSIZE);

    extern char user_code[];
    memcpy((void*) 0xff0000, user_code, PGSIZE);

    memset(&t->tf, 0, sizeof(t->tf));
    t->tf.tf_ss = GD_UD | 3;
    t->tf.tf_rsp = ULIM;
    t->tf.tf_rflags = 0;
    t->tf.tf_cs = GD_UT | 3;
    t->tf.tf_rip = 0xff0000;
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
}

void
thread_run(struct Thread *t)
{
    lcr3(t->cr3);
    trapframe_pop(&t->tf);
}
