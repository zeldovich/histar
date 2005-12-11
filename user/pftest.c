#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/memlayout.h>

int
main(int ac, char **av)
{
    uint64_t ct = start_arg;

    struct cobj_ref seg;
    assert(0 == segment_alloc(ct, PGSIZE, &seg));
    void *va;
    assert(0 == segment_map(ct, seg, 1, &va, 0));

    struct ulabel ul = {
	.ul_size = 32,
	.ul_ent = va,
    };

    cprintf("Trying to get label..\n");
    assert(0 == thread_get_label(ct, &ul));

    void *va2;
    assert(0 == segment_map(ct, seg, 0, &va2, 0));
    ul.ul_ent = va2;

    cprintf("Trying to get label into RO page\n");
    assert(0 == thread_get_label(ct, &ul));
}
