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

    for (;;) {
	sys_thread_sleep(1000);
	cprintf("Trying to get label..\n");
	assert(0 == thread_get_label(ct, &ul));
    }
}
