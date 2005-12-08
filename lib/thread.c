#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>

int
thread_create(int container, void (*entry)(void*), void *arg, struct cobj_ref *threadp)
{
    int stacksize = 2 * PGSIZE;
    struct cobj_ref stack;
    int r = segment_alloc(container, stacksize, &stack);
    if (r < 0)
	return r;

    void *stackbase;
    r = segment_map(container, stack, 1, &stackbase, 0);
    if (r < 0) {
	sys_obj_unref(stack);
	return r;
    }

    struct thread_entry e;
    r = sys_segment_get_map(&e.te_segmap);
    if (r < 0) {
	segment_unmap(container, stackbase);
	sys_obj_unref(stack);
	return r;
    }

    e.te_entry = entry;
    e.te_stack = stackbase + stacksize;
    e.te_arg = (uint64_t) arg;

    int tslot = sys_thread_create(container);
    if (tslot < 0) {
	segment_unmap(container, stackbase);
	sys_obj_unref(stack);
	return tslot;
    }

    *threadp = COBJ(container, tslot);
    r = sys_thread_start(*threadp, &e);
    if (r < 0) {
	segment_unmap(container, stackbase);
	sys_obj_unref(stack);
	sys_obj_unref(*threadp);
	return r;
    }

    return 0;
}
