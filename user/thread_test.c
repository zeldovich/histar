#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>

static volatile uint64_t counter;

static void
thread_entry(uint64_t arg)
{
    cprintf("thread_test: thread_entry\n");
    counter += 3;
    sys_halt();

    panic("thread_entry: still alive after sys_halt");
}

int
main(int ac, char **av)
{
    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int pm = sys_container_store_cur_pmap(rc, 0);
    if (pm < 0)
	panic("cannot store cur_pm: %d", pm);

    // XXX if we could get a user-space header defining ULIM...
    char *stacktop = (void*) 0x700000000000;
    int sg = sys_segment_create(rc, 1);
    if (sg < 0)
	panic("cannot create stack segment: %d", sg);

    int r = sys_segment_map(COBJ(rc, sg), COBJ(rc, pm), stacktop - 4096, 0, 1, segment_map_cow);
    if (r < 0)
	panic("cannot map stack segment: %d", r);

    int g = sys_gate_create(rc, &thread_entry, stacktop, COBJ(rc, pm));
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
