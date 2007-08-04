#include <stdio.h>
#include <machine/um.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/timer.h>

static void
test_stuff(void)
{
    struct Label *l1, *l2;
    assert(0 == label_alloc(&l1, 1));
    assert(0 == label_alloc(&l2, 1));

    uint64_t t0, t1;
    uint64_t count = 1000000;

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == label_compare(l1, l2, label_leq_starlo, 0));
    t1 = timer_user_nsec();
    cprintf("Non-cached label comparison: %"PRIu64" nsec\n", (t1 - t0) / count);

    t0 = timer_user_nsec();
    for (uint64_t i = 0; i < count; i++)
	assert(0 == label_compare(l1, l2, label_leq_starlo, 1));
    t1 = timer_user_nsec();
    cprintf("Cached label comparison: %"PRIu64" nsec\n", (t1 - t0) / count);
}

int
main(int ac, char **av)
{
    printf("HiStar/um: starting..\n");

    uint32_t mem_bytes = 4096 * 4096;

    um_cons_init();
    um_mem_init(mem_bytes);
    um_time_init();

    kobject_init();
    sched_init();
    pstate_init();
    prof_init();

    cprintf("Ready.\n");
    test_stuff();
}
