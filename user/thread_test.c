#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>
#include <inc/lib.h>

static volatile uint64_t counter = 2;

static void
thread_entry(void *varg)
{
    uint64_t arg = (uint64_t) varg;

    printf("thread_test: thread_entry: %ld\n", arg);
    counter += arg;
    sys_thread_halt();

    panic("thread_entry: still alive after sys_halt");
}

int
main(int ac, char **av)
{
    uint64_t old_counter = counter;

    int c_root = start_env->container;
    struct cobj_ref t1, t2;

    int r = thread_create(c_root, &thread_entry, (void*)3, &t1);
    if (r < 0)
	printf("cannot create thread 1: %s\n", e2s(r));

    r = thread_create(c_root, &thread_entry, (void*)4, &t2);
    if (r < 0)
	printf("cannot create thread 2: %s\n", e2s(r));

    printf("thread_test: watching counter, currently at %ld\n", old_counter);
    for (;;) {
	uint64_t counter_save = counter;
	if (counter_save != old_counter) {
	    printf("thread_test: counter changed: %ld -> %ld\n",
		   old_counter, counter_save);
	    old_counter = counter_save;
	}

	if (counter_save == 9)
	    break;
    }

    assert(0 == sys_obj_unref(t1));
    assert(0 == sys_obj_unref(t2));

    printf("thread_test: GC'ed thread slots, exiting\n");
    return 0;
}
