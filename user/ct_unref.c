#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int sci = sys_container_alloc(rc);
    if (sci < 0)
	panic("cannot allocate sub-container: %s", e2s(sci));

    int64_t sc = sys_obj_get_id(COBJ(rc, sci));
    if (sc < 0)
	panic("cannot get sub-container: %d", e2s(sc));

    int r = sys_container_store_cur_thread(sc);
    if (r < 0)
	panic("cannot addref current thread: %s", e2s(r));

    for (int i = 1; i < 10; i++) {
	kobject_type_t t = sys_obj_get_type(COBJ(rc, i));
	cprintf("<%d:%d> type %s\n", rc, i,
				     t == kobj_thread ? "thread" :
				     t == kobj_container ? "container" :
				     t == kobj_segment ? "segment" :
				     t == kobj_none ? "none" : "other");

	if (i != sci)
	    sys_container_unref(COBJ(rc, i));
    }
    cprintf("ct_unref now dropping sub-container\n");
    sys_container_unref(COBJ(rc, sci));

    panic("ct_unref: still alive, strange");
}
