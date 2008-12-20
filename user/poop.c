#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/syscall.h>

#include <inttypes.h>

enum { iters = 1000000 };

uint64_t bleh;

static void
thread_entry(void *x)
{
    uint64_t s = sys_clock_nsec();
    for (int i = 0; i < iters - 1; i++) {
	bleh = i * 2 + 1;
	sys_sync_wakeup(&bleh);
	sys_sync_wait(&bleh, i * 2 + 1, ~0);
    }
    uint64_t e = sys_clock_nsec();
    cprintf("%"PRIu64"\n", (e - s) / ((iters - 1) * 1000));

    sys_sync_wakeup(&bleh);
    return;
}

int
main(int ac, char **av)
{
    int r;
    struct cobj_ref tref;
    r = thread_create(start_arg0, thread_entry, 0, &tref, "poop");
    for (int i = 0; i < iters; i++) {
	bleh = i * 2;
	sys_sync_wakeup(&bleh);
	sys_sync_wait(&bleh, i * 2, ~0);
    }
    return 0;
}
