#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>

static volatile uint64_t counter;

static void
thread_entry(uint64_t bump)
{
    cprintf("thread_test: thread_entry: %lx\n", bump);
    counter += bump;
    sys_halt();
}

int
main(int ac, char **av)
{
    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int as = sys_container_store_cur_addrspace(rc, 0);
    if (as < 0)
	panic("cannot store cur_as: %d", as);

    int g = sys_gate_create(rc, &thread_entry, 3, COBJ(rc, as));
    if (g < 0)
	panic("cannot create gate: %d", g);

    counter = 2;
    uint64_t old_counter = counter;

    int t1 = sys_thread_create(rc, COBJ(rc, g));
    if (t1 < 0)
	panic("cannot create thread 1: %d", t1);

    int t2 = sys_thread_create(rc, COBJ(rc, g));
    if (t2 < 0)
	panic("cannot create thread 2: %d", t2);

    cprintf("thread_test: watching counter, currently at %d\n", counter);
    for (;;) {
	uint64_t counter_save = counter;
	if (counter_save != old_counter) {
	    cprintf("thread_test: counter changed: %d -> %d\n", old_counter, counter_save);
	    old_counter = counter_save;
	}

	if (counter_save == 8)
	    break;
    }

    assert(0 == sys_container_unref(COBJ(rc, t1)));
    assert(0 == sys_container_unref(COBJ(rc, t2)));

    cprintf("thread_test: GC'ed thread slots, exiting\n");
    return 0;
}
