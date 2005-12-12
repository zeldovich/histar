#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>

int
thread_create(uint64_t container, void (*entry)(void*), void *arg, struct cobj_ref *threadp)
{
    int stacksize = 2 * PGSIZE;
    struct cobj_ref stack;
    void *stackbase;
    int r = segment_alloc(container, stacksize, &stack, &stackbase);
    if (r < 0)
	return r;

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

    int64_t tid = sys_thread_create(container);
    if (tid < 0) {
	segment_unmap(container, stackbase);
	sys_obj_unref(stack);
	return tid;
    }

    *threadp = COBJ(container, tid);
    r = sys_thread_start(*threadp, &e);
    if (r < 0) {
	segment_unmap(container, stackbase);
	sys_obj_unref(stack);
	sys_obj_unref(*threadp);
	return r;
    }

    return 0;
}

uint64_t
thread_id(void)
{
    int64_t tid = sys_thread_id();
    if (tid < 0)
	panic("sys_thread_id: %s", e2s(tid));
    return tid;
}

void
thread_halt(void)
{
    sys_thread_halt();
    panic("halt: still alive");
}

int
thread_get_label(uint64_t ctemp, struct ulabel *ul)
{
    uint64_t tid = thread_id();
    int r = sys_thread_addref(ctemp);
    if (r < 0)
	return r;

    r = sys_obj_get_label(COBJ(ctemp, tid), ul);
    sys_obj_unref(COBJ(ctemp, tid));
    return r;
}
