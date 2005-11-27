#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int sci = sys_container_alloc(rc);
    if (sci < 0) {
	cprintf("cannot allocate sub-container: %d\n", sci);
	sys_halt();
    }

    int64_t sc = sys_container_get_c_idx(rc, sci);
    if (sc < 0) {
	cprintf("cannot get sub-container: %d\n", sc);
	sys_halt();
    }

    int r = sys_thread_addref(sc);
    if (r < 0) {
	cprintf("cannot addref current thread: %d\n", r);
	sys_halt();
    }

    int i;
    for (i = 1; i < 10; i++) {
	container_object_type t = sys_container_get_type(rc, i);
	cprintf("<%d:%d> type %s\n", rc, i,
				     t == cobj_thread ? "thread" :
				     t == cobj_container ? "container" :
				     t == cobj_none ? "none" : "other");

	if (t == cobj_thread) {
	    cprintf("unref'ing <%d:%d>\n", rc, i);
	    sys_container_unref(rc, i);
	}
    }
    cprintf("ct_unref now dropping sub-container\n");
    sys_container_unref(rc, sci);

    cprintf("ct_unref: still alive, strange\n");
    return 0;
}
