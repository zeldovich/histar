#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>
#include <inc/memlayout.h>

static volatile uint64_t counter;

static void
thread_entry(uint64_t arg)
{
    cprintf("thread_test: thread_entry: %d\n", arg);
    counter += arg;
    sys_halt();

    panic("thread_entry: still alive after sys_halt");
}

int
main(int ac, char **av)
{
    // XXX
    // if we don't COW our .data section upfront, then all
    // children get a COW version of the .data section, and
    // it's not shared with us anymore!
    counter = 2;

    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int pm = sys_container_store_cur_pmap(rc, 0);
    if (pm < 0)
	panic("cannot store cur_pm: %d", pm);

    int sg = sys_segment_create(rc, 1);
    if (sg < 0)
	panic("cannot create stack segment: %d", sg);

    char *stacktop1 = (void*) 0x710000000000;
    char *stacktop2 = (void*) 0x720000000000;
    assert(0 == sys_segment_map(COBJ(rc, sg), COBJ(rc, pm), stacktop1 - PGSIZE, 0, 1, segment_map_cow));
    assert(0 == sys_segment_map(COBJ(rc, sg), COBJ(rc, pm), stacktop2 - PGSIZE, 0, 1, segment_map_cow));

    uint64_t old_counter = counter;

    struct thread_entry e = {
	.te_pmap = COBJ(rc, pm),
	.te_pmap_copy = 0,
	.te_entry = &thread_entry,
	.te_stack = stacktop1,
	.te_arg = 3
    };

    int t1 = sys_thread_create(rc);
    if (t1 < 0)
	panic("cannot create thread 1: %d", t1);

    int t2 = sys_thread_create(rc);
    if (t2 < 0)
	panic("cannot create thread 2: %d", t2);

    assert(0 == sys_thread_start(COBJ(rc, t1), &e));
    e.te_stack = stacktop2;
    assert(0 == sys_thread_start(COBJ(rc, t2), &e));

    cprintf("thread_test: watching counter, currently at %d\n", old_counter);
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
